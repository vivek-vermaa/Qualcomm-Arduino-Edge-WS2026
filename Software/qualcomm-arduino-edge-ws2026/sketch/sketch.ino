// SPDX-FileCopyrightText: Copyright (C) Electronic Cats
//
// SPDX-License-Identifier: MPL-2.0

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
#include <Arduino_RouterBridge.h>

#define NEODRIVER_ADDR  0x60
#define NEO_PIN         15
#define NUM_PIXELS      5

seesaw_NeoPixel strip(NUM_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800, &Wire1);
bool ledsOn = false;

void setAll(uint32_t color) {
  for (int i = 0; i < NUM_PIXELS; i++) strip.setPixelColor(i, color);
  strip.show();
}

void setup() {
  if (!strip.begin(NEODRIVER_ADDR)) {
    while (1) delay(10);
  }
  strip.setBrightness(50);
  setAll(0x000000);

  Bridge.begin();
  Bridge.provide("keyword_detected", wake_up);
}

void loop() {}

void wake_up() {
  ledsOn = !ledsOn;
  if (ledsOn) {
    setAll(strip.Color(0, 200, 0));
  } else {
    setAll(0x000000);
  }
}
