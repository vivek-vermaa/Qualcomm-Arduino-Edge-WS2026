// SPDX-FileCopyrightText: Copyright (C) Electronic Cats
// SPDX-License-Identifier: MPL-2.0
// Modified and extended by Copyright (c) Vivek Verma - 2026

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <zephyr/kernel.h>
#include "heart_frames.h"

#define PIN_RED    2
#define PIN_BLUE   3
#define PIN_GREEN  4

#define KEYWORD_STACK_SIZE   2048
#define SENSOR_STACK_SIZE    2048
#define KEYWORD_THREAD_PRIO  2
#define SENSOR_THREAD_PRIO   5
#define SENSOR_INTERVAL_MS   10000

K_THREAD_STACK_DEFINE(keyword_stack, KEYWORD_STACK_SIZE);
K_THREAD_STACK_DEFINE(sensor_stack,  SENSOR_STACK_SIZE);

static struct k_thread keyword_thread;
static struct k_thread sensor_thread;

struct keyword_msg { char word[16]; };
K_MSGQ_DEFINE(keyword_queue, sizeof(struct keyword_msg), 8, 4);
K_MUTEX_DEFINE(display_mutex);

Arduino_LED_Matrix matrix;
Adafruit_BME680 bme;
bool bmeReady = false;
uint8_t frame[8][12];

void clearFrame() { memset(frame, 0, sizeof(frame)); }
void px(int x, int y) {
  if (x >= 0 && x < 12 && y >= 0 && y < 8) frame[y][x] = 1;
}
void pushFrame() { matrix.renderBitmap(frame, 8, 12); }

void drawV() {
  clearFrame();
  px(0,0); px(11,0); px(0,1); px(11,1);
  px(1,2); px(10,2); px(1,3); px(10,3);
  px(2,4); px(9,4);  px(3,5); px(8,5);
  px(4,6); px(7,6);  px(5,7); px(6,7);
  pushFrame();
}
void drawR() {
  clearFrame();
  for (int y = 0; y <= 7; y++) px(0, y);
  for (int x = 1; x <= 6; x++) px(x, 0);
  px(7,1); px(7,2); px(7,3);
  for (int x = 1; x <= 6; x++) px(x, 4);
  px(3,5); px(4,6); px(5,7);
  pushFrame();
}
void drawB() {
  clearFrame();
  for (int y = 0; y <= 7; y++) px(0, y);
  for (int x = 1; x <= 5; x++) { px(x,0); px(x,4); px(x,7); }
  px(6,1); px(6,2); px(6,3); px(6,5); px(6,6);
  pushFrame();
}
void drawG() {
  clearFrame();
  for (int x = 1; x <= 7; x++) { px(x,0); px(x,7); }
  for (int y = 1; y <= 6; y++) px(0, y);
  for (int x = 4; x <= 7; x++) px(x, 4);
  px(7,5); px(7,6);
  pushFrame();
}

void allLEDsOff() {
  digitalWrite(PIN_RED,   LOW);
  digitalWrite(PIN_BLUE,  LOW);
  digitalWrite(PIN_GREEN, LOW);
}

void blinkThenStay(int pin) {
  allLEDsOff();
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin, HIGH); k_msleep(150);
    digitalWrite(pin, LOW);  k_msleep(150);
  }
  digitalWrite(pin, HIGH);
}

void readAndPrintSensor() {
  if (!bmeReady) {
    printk("[BME680] Not available\n");
    return;
  }
  if (!bme.performReading()) {
    printk("[BME680] Reading failed\n");
    return;
  }
  printk("========= BME680 =========\n");
  printk("  Temperature : %d.%02d C\n",
         (int)bme.temperature, abs((int)(bme.temperature * 100) % 100));
  printk("  Humidity    : %d.%02d %%\n",
         (int)bme.humidity, abs((int)(bme.humidity * 100) % 100));
  printk("  Pressure    : %d.%02d hPa\n",
         (int)(bme.pressure / 100.0),
         abs((int)(bme.pressure) % 100));
  printk("  Gas         : %d.%02d KOhms\n",
         (int)(bme.gas_resistance / 1000.0),
         abs((int)(bme.gas_resistance / 10) % 100));
  printk("==========================\n");
}

void enqueue(const char* word) {
  struct keyword_msg msg;
  strncpy(msg.word, word, sizeof(msg.word) - 1);
  msg.word[sizeof(msg.word) - 1] = '\0';
  if (k_msgq_put(&keyword_queue, &msg, K_NO_WAIT) != 0) {
    printk("[WARN] keyword queue full\n");
  } else {
    printk("[ENQUEUE] %s\n", word);
  }
}

void cmd_vivek() { enqueue("vivek"); }
void cmd_red()   { enqueue("red");   }
void cmd_blue()  { enqueue("blue");  }
void cmd_green() { enqueue("green"); }

void keyword_thread_fn(void*, void*, void*) {
  struct keyword_msg msg;
  printk("[THREAD] keyword thread started\n");

  while (1) {
    if (k_msgq_get(&keyword_queue, &msg, K_FOREVER) == 0) {
      printk("[KEYWORD] processing: %s\n", msg.word);
      k_mutex_lock(&display_mutex, K_FOREVER);
      if      (strcmp(msg.word, "vivek") == 0) { drawV(); }
      else if (strcmp(msg.word, "red")   == 0) { drawR(); blinkThenStay(PIN_RED);   }
      else if (strcmp(msg.word, "blue")  == 0) { drawB(); blinkThenStay(PIN_BLUE);  }
      else if (strcmp(msg.word, "green") == 0) { drawG(); blinkThenStay(PIN_GREEN); }
      k_mutex_unlock(&display_mutex);
      readAndPrintSensor();
    }
  }
}

void sensor_thread_fn(void*, void*, void*) {
  printk("[THREAD] sensor thread started\n");
  while (1) {
    k_msleep(SENSOR_INTERVAL_MS);
    printk("[PERIODIC] sensor read\n");
    readAndPrintSensor();
  }
}

void setup() {
  k_msleep(3000);
  printk("=== UNO Q Zephyr starting ===\n");

  pinMode(PIN_RED,   OUTPUT);
  pinMode(PIN_BLUE,  OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  allLEDsOff();

  matrix.begin();
  matrix.loadFrame(HeartStatic);
  printk("[INFO] LED Matrix ready\n");

  Wire.begin();
  if (bme.begin(0x77) || bme.begin(0x76)) {
    bmeReady = true;
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    printk("[INFO] BME680 ready\n");
  } else {
    printk("[WARN] BME680 not found\n");
  }

  k_thread_create(&keyword_thread, keyword_stack, KEYWORD_STACK_SIZE,
                  keyword_thread_fn, NULL, NULL, NULL,
                  KEYWORD_THREAD_PRIO, 0, K_NO_WAIT);
  k_thread_name_set(&keyword_thread, "keyword");

  k_thread_create(&sensor_thread, sensor_stack, SENSOR_STACK_SIZE,
                  sensor_thread_fn, NULL, NULL, NULL,
                  SENSOR_THREAD_PRIO, 0, K_NO_WAIT);
  k_thread_name_set(&sensor_thread, "sensor");

  printk("[INFO] Zephyr threads launched\n");

  Bridge.begin();
  Bridge.provide("vivek", cmd_vivek);
  Bridge.provide("red",   cmd_red);
  Bridge.provide("blue",  cmd_blue);
  Bridge.provide("green", cmd_green);
  printk("[INFO] Ready — say: vivek | red | blue | green\n");
}

void loop() {
  static int64_t lastPing = 0;
  int64_t now = k_uptime_get();
  if (now - lastPing >= 5000) {
    printk("[PING] alive t=%lld\n", now);
    lastPing = now;
  }
  k_msleep(10);
}