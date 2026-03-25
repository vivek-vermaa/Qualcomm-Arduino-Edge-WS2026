// SPDX-FileCopyrightText: Copyright (C) Electronic Cats
//
// SPDX-License-Identifier: MPL-2.0

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h>
#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>

#include "heart_frames.h"

Arduino_LED_Matrix matrix;

#define NEODRIVER_ADDR  0x60
#define NEO_PIN         15
#define NUM_PIXELS      5

seesaw_NeoPixel strip(NUM_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800, &Wire1);

bool neoReady = false;
uint8_t brightness = 50;
uint8_t r = 255, g = 255, b = 255;

void setAll(uint8_t red, uint8_t green, uint8_t blue) {
  if (!neoReady) return;
  for (int i = 0; i < NUM_PIXELS; i++)
    strip.setPixelColor(i, strip.Color(red, green, blue));
  strip.show();
}

void setup() {
  matrix.begin();
  matrix.clear();
  matrix.loadFrame(HeartStatic);

  neoReady = strip.begin(NEODRIVER_ADDR);
  if (neoReady) {
    strip.setBrightness(brightness);
    setAll(r, g, b);
  }

  Bridge.begin();
  Bridge.provide("warmer_light", warmer_light);
  Bridge.provide("cooler_light", cooler_light);
  Bridge.provide("dimmer", dimmer);
  Bridge.provide("brighter", brighter);
}

void loop() {}

void animateHeart() {
  matrix.loadSequence(HeartAnim);
  matrix.playSequence();
  delay(1000);
  matrix.loadFrame(HeartStatic);
}

void warmer_light() {
  r = 255; g = 194; b = 138;
  setAll(r, g, b);
  animateHeart();
}

void cooler_light() {
  r = 144; g = 213; b = 255;
  setAll(r, g, b);
  animateHeart();
}

void dimmer() {
  brightness = (uint8_t)max(1, (int)(brightness * 0.6));
  if (neoReady) strip.setBrightness(brightness);
  setAll(r, g, b);
  animateHeart();
}

void brighter() {
  brightness = (uint8_t)min(255, (int)(brightness * 1.4));
  if (neoReady) strip.setBrightness(brightness);
  setAll(r, g, b);
  animateHeart();
}
