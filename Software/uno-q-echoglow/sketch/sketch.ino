// SPDX-FileCopyrightText: Copyright (C) Electronic Cats
// SPDX-License-Identifier: MPL-2.0

// Modified and extended by Copyright (c) Vivek Verma -2026
// Forked to explore real-time edge AI on Arduino UNO Q
// Focused on building systems that can listen, understand, and respond locally
// without cloud dependency, bridging embedded systems with intelligent behavior

#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include "heart_frames.h"

#define PIN_RED    2
#define PIN_BLUE   3
#define PIN_GREEN  4

Arduino_LED_Matrix matrix;
Adafruit_BME680 bme;
bool bmeReady = false;

// ── 12×8 frame buffer ─────────────────────────────────────────────────────────
uint8_t frame[8][12];

void clearFrame() { memset(frame, 0, sizeof(frame)); }

void px(int x, int y) {
  if (x >= 0 && x < 12 && y >= 0 && y < 8)
    frame[y][x] = 1;
}

void pushFrame() { matrix.renderBitmap(frame, 8, 12); }

// ── Letters drawn pixel by pixel ─────────────────────────────────────────────
// Matrix is 12 wide x 8 tall
// Letters are 5 wide x 7 tall, starting at x=3 to center them

void drawV() {
  clearFrame();
  // col 0 and col 4 = rows 0-4, converging to col 2 at rows 5-6
  px(3,0); px(7,0);
  px(3,1); px(7,1);
  px(3,2); px(7,2);
  px(4,3); px(6,3);
  px(4,4); px(6,4);
  px(5,5);
  px(5,6);
  pushFrame();
}

void drawR() {
  clearFrame();
  // vertical bar left
  for (int y = 0; y <= 6; y++) px(3, y);
  // top bar
  px(4,0); px(5,0); px(6,0);
  // right side of top bump
  px(7,1); px(7,2);
  // middle bar
  px(4,3); px(5,3); px(6,3);
  // diagonal leg
  px(5,4); px(6,5); px(7,6);
  pushFrame();
}

void drawB() {
  clearFrame();
  // vertical bar left
  for (int y = 0; y <= 6; y++) px(3, y);
  // top bar
  px(4,0); px(5,0); px(6,0);
  // right side top bump
  px(7,1); px(7,2);
  // middle bar
  px(4,3); px(5,3); px(6,3);
  // right side bottom bump
  px(7,4); px(7,5);
  // bottom bar
  px(4,6); px(5,6); px(6,6);
  pushFrame();
}

void drawG() {
  clearFrame();
  // top arc
  px(4,0); px(5,0); px(6,0);
  // left side
  px(3,1); px(3,2);
  // middle shelf
  px(3,3); px(5,3); px(6,3); px(7,3);
  // right side
  px(7,4); px(7,5);
  // bottom arc
  px(4,6); px(5,6); px(6,6);
  // close left bottom
  px(3,4); px(3,5);
  pushFrame();
}

// ── LED helpers ───────────────────────────────────────────────────────────────
void allLEDsOff() {
  digitalWrite(PIN_RED,   LOW);
  digitalWrite(PIN_BLUE,  LOW);
  digitalWrite(PIN_GREEN, LOW);
}

void blinkThenStay(int pin) {
  allLEDsOff();
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin, HIGH); delay(150);
    digitalWrite(pin, LOW);  delay(150);
  }
  digitalWrite(pin, HIGH);
}

// ── BME680 ────────────────────────────────────────────────────────────────────
void readAndPrintSensor() {
  if (!bmeReady) {
    Serial.println("[BME680] Not available — check SDA=A4 SCL=A5");
    return;
  }
  if (!bme.performReading()) {
    Serial.println("[BME680] Reading failed");
    return;
  }
  Serial.println("========= BME680 =========");
  Serial.print("  Temperature : "); Serial.print(bme.temperature);           Serial.println(" C");
  Serial.print("  Humidity    : "); Serial.print(bme.humidity);               Serial.println(" %");
  Serial.print("  Pressure    : "); Serial.print(bme.pressure / 100.0);      Serial.println(" hPa");
  Serial.print("  Gas         : "); Serial.print(bme.gas_resistance / 1000.0); Serial.println(" KOhms");
  Serial.println("==========================");
}

// ── Command handlers ──────────────────────────────────────────────────────────
void cmd_vivek() {
  Serial.println("[CMD] VIVEK — showing V");
  drawV();
  readAndPrintSensor();
}

void cmd_red() {
  Serial.println("[CMD] RED — showing R");
  drawR();
  blinkThenStay(PIN_RED);
  readAndPrintSensor();
}

void cmd_blue() {
  Serial.println("[CMD] BLUE — showing B");
  drawB();
  blinkThenStay(PIN_BLUE);
  readAndPrintSensor();
}

void cmd_green() {
  Serial.println("[CMD] GREEN — showing G");
  drawG();
  blinkThenStay(PIN_GREEN);
  readAndPrintSensor();
}

// ── Setup / Loop ──────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  delay(3000);

  pinMode(PIN_RED,   OUTPUT);
  pinMode(PIN_BLUE,  OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  allLEDsOff();

  matrix.begin();
  matrix.loadFrame(HeartStatic);
  Serial.println("[INFO] LED Matrix ready");

  Wire.begin();
  if (bme.begin()) {
    bmeReady = true;
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println("[INFO] BME680 ready");
  } else {
    Serial.println("[WARN] BME680 not found");
  }

  Bridge.begin();
  Bridge.provide("vivek", cmd_vivek);
  Bridge.provide("red",   cmd_red);
  Bridge.provide("blue",  cmd_blue);
  Bridge.provide("green", cmd_green);
  Serial.println("[INFO] Ready — say: vivek | red | blue | green");
}

void loop() {}
