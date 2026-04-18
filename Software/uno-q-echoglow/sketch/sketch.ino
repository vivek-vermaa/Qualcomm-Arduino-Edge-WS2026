// SPDX-FileCopyrightText: Copyright (C) Electronic Cats
// SPDX-License-Identifier: MPL-2.0
// Modified and extended by Copyright (c) Vivek Verma - 2026
/*
   History: 

    18.04.2026:  Upgrade: Robust Word Detection, Command Dictionary, & Visual States
    18.04.2026:  Adding Zephr RTOS Support.
*/


#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <zephyr/kernel.h>
#include <ctype.h>
#include "heart_frames.h"

//  PWM Pins for Cloud9 effect :)
#define PIN_RED    3
#define PIN_GREEN  5
#define PIN_BLUE   6

// Thread Configuration
#define KEYWORD_STACK_SIZE   2048
#define SENSOR_STACK_SIZE    2048
#define KEYWORD_THREAD_PRIO  2
#define SENSOR_THREAD_PRIO   5
#define SENSOR_INTERVAL_MS   10000

K_THREAD_STACK_DEFINE(keyword_stack, KEYWORD_STACK_SIZE);
K_THREAD_STACK_DEFINE(sensor_stack,  SENSOR_STACK_SIZE);

static struct k_thread keyword_thread;
static struct k_thread sensor_thread;

struct keyword_msg { char word[32]; };
K_MSGQ_DEFINE(keyword_queue, sizeof(struct keyword_msg), 8, 4);
K_MUTEX_DEFINE(display_mutex);

Arduino_LED_Matrix matrix;
Adafruit_BME680 bme;
bool bmeReady = false;
uint8_t frame[8][12];

// ================= HARDWARE CONTROL =================

void allLEDsOff() {
  analogWrite(PIN_RED, 0);
  analogWrite(PIN_GREEN, 0);
  analogWrite(PIN_BLUE, 0);
}

void clearFrame() { memset(frame, 0, sizeof(frame)); }
void pushFrame()  { matrix.renderBitmap(frame, 8, 12); }
void px(int x, int y) { if (x >= 0 && x < 12 && y >= 0 && y < 8) frame[y][x] = 1; }

void allOff() {
  allLEDsOff();
  clearFrame();
  pushFrame();
}

void blinkAndPowerOff(int pin) {
  allLEDsOff();
  
  // Use analogWrite for HIGH (255) and LOW (0) to prevent timer conflicts
  for (int i = 0; i < 3; i++) {
    analogWrite(pin, 255); 
    k_msleep(150);
    analogWrite(pin, 0);   
    k_msleep(150);
  }
  
  // Stay on for the remainder of 3 seconds
  analogWrite(pin, 255);
  k_msleep(2100); 
  
  allOff();
}

void rainbowEffect(int delayMs = 40) {
  for (int i = 0; i <= 255; i += 10) { analogWrite(PIN_RED, 255); analogWrite(PIN_GREEN, i); analogWrite(PIN_BLUE, 0); k_msleep(delayMs); }
  for (int i = 255; i >= 0; i -= 10) { analogWrite(PIN_RED, i); analogWrite(PIN_GREEN, 255); analogWrite(PIN_BLUE, 0); k_msleep(delayMs); }
  for (int i = 0; i <= 255; i += 10) { analogWrite(PIN_RED, 0); analogWrite(PIN_GREEN, 255); analogWrite(PIN_BLUE, i); k_msleep(delayMs); }
  for (int i = 255; i >= 0; i -= 10) { analogWrite(PIN_RED, i); analogWrite(PIN_GREEN, 0); analogWrite(PIN_BLUE, 255); k_msleep(delayMs); }
  allOff(); 
}

// ================= VISUAL STATES =================

void drawV() { clearFrame(); px(0,0); px(11,0); px(0,1); px(11,1); px(1,2); px(10,2); px(1,3); px(10,3); px(2,4); px(9,4); px(3,5); px(8,5); px(4,6); px(7,6); px(5,7); px(6,7); pushFrame(); }
void drawR() { clearFrame(); for(int y=0;y<=7;y++) px(0,y); for(int x=1;x<=6;x++) px(x,0); px(7,1); px(7,2); px(7,3); for(int x=1;x<=6;x++) px(x,4); px(3,5); px(4,6); px(5,7); pushFrame(); }
void drawB() { clearFrame(); for(int y=0;y<=7;y++) px(0,y); for(int x=1;x<=5;x++){ px(x,0); px(x,4); px(x,7); } px(6,1); px(6,2); px(6,3); px(6,5); px(6,6); pushFrame(); }
void drawG() { clearFrame(); for(int x=1;x<=7;x++){ px(x,0); px(x,7); } for(int y=1;y<=6;y++) px(0,y); for(int x=4;x<=7;x++) px(x,4); px(7,5); px(7,6); pushFrame(); }

void drawQuestion() { clearFrame(); px(5,1); px(6,1); px(4,2); px(7,2); px(7,3); px(6,4); px(5,5); px(5,7); pushFrame(); k_msleep(2000); allOff(); }
void drawIdle() { clearFrame(); px(0,0); pushFrame(); } // A single pixel indicating system is alive

// ================= COMMAND DICTIONARY =================

// Define a function pointer type for actions
typedef void (*ActionFunc)();

// Map words to their visual and LED actions
struct Command {
  const char* keyword;
  ActionFunc matrixAction;
  int pinToBlink; // -1 if not a simple blink
};

void actionVivek() { drawV(); rainbowEffect(30); }
void actionRed()   { drawR(); blinkAndPowerOff(PIN_RED); }
void actionBlue()  { drawB(); blinkAndPowerOff(PIN_BLUE); }
void actionGreen() { drawG(); blinkAndPowerOff(PIN_GREEN); }

// The Dictionary: Add new words easily right here!
const Command dictionary[] = {
  {"vivek", actionVivek, -1},
  {"red",   actionRed,   PIN_RED},
  {"blue",  actionBlue,  PIN_BLUE},
  {"green", actionGreen, PIN_GREEN}
};
const int dictSize = sizeof(dictionary) / sizeof(dictionary[0]);

// ================= WORD DETECTION ENGINE =================

// Enqueue raw strings from Bridge or Serial
void enqueue(const char* word) {
  struct keyword_msg msg;
  strncpy(msg.word, word, sizeof(msg.word)-1);
  msg.word[sizeof(msg.word)-1] = '\0';
  k_msgq_put(&keyword_queue, &msg, K_NO_WAIT);
}

// Router callbacks
void cmd_vivek() { enqueue("vivek"); }
void cmd_red()   { enqueue("red"); }
void cmd_blue()  { enqueue("blue"); }
void cmd_green() { enqueue("green"); }

// Sanitizes the string: forces lowercase and removes trailing spaces/newlines
void sanitizeString(char* str) {
  int len = strlen(str);
  while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\n' || str[len-1] == '\r')) {
    str[len-1] = '\0';
    len--;
  }
  for (int i = 0; i < len; i++) {
    str[i] = tolower(str[i]);
  }
}

// ================= THREADS =================

void keyword_thread_fn(void*, void*, void*) {
  struct keyword_msg msg;

  while (1) {
    // Show idle pixel while waiting
    k_mutex_lock(&display_mutex, K_FOREVER);
    drawIdle();
    k_mutex_unlock(&display_mutex);

    if (k_msgq_get(&keyword_queue, &msg, K_FOREVER) == 0) {
      
      sanitizeString(msg.word);
      printk("Detected Word: [%s]\n", msg.word); // Debug output

      k_mutex_lock(&display_mutex, K_FOREVER);
      
      bool found = false;
      for (int i = 0; i < dictSize; i++) {
        if (strcmp(msg.word, dictionary[i].keyword) == 0) {
          found = true;
          // Execute the mapped function
          dictionary[i].matrixAction(); 
          break;
        }
      }

      // If word isn't in dictionary, show error
      if (!found) {
        printk("Unknown command!\n");
        drawQuestion();
      }

      k_mutex_unlock(&display_mutex);
    }
  }
}

void sensor_thread_fn(void*, void*, void*) {
  while (1) {
    k_msleep(SENSOR_INTERVAL_MS);
    if (bmeReady && bme.performReading()) {
      printk("Temp: %d C | Hum: %d %%\n", (int)bme.temperature, (int)bme.humidity);
    }
  }
}

// ================= ARDUINO CORE =================

void setup() {
  Serial.begin(115200);
  k_msleep(2000);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  allOff();

  matrix.begin();
  Wire.begin();
  
  if (bme.begin(0x77) || bme.begin(0x76)) {
    bmeReady = true;
  }

  // Start Threads
  k_thread_create(&keyword_thread, keyword_stack, KEYWORD_STACK_SIZE,
                  keyword_thread_fn, NULL, NULL, NULL,
                  KEYWORD_THREAD_PRIO, 0, K_NO_WAIT);

  k_thread_create(&sensor_thread, sensor_stack, SENSOR_STACK_SIZE,
                  sensor_thread_fn, NULL, NULL, NULL,
                  SENSOR_THREAD_PRIO, 0, K_NO_WAIT);

  // Setup Bridge
  Bridge.begin();
  Bridge.provide("vivek", cmd_vivek);
  Bridge.provide("red",   cmd_red);
  Bridge.provide("blue",  cmd_blue);
  Bridge.provide("green", cmd_green);
}

void loop() {
  // Serial Fallback: Allows testing words directly via Serial Monitor
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    enqueue(input.c_str());
  }
  
  k_msleep(50); // Yield to RTOS
}