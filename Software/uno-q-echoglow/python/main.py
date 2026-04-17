# SPDX-FileCopyrightText: Copyright (C) Electronic Cats
# SPDX-License-Identifier: MPL-2.0

from arduino.app_utils import *
from arduino.app_bricks.keyword_spotting import KeywordSpotting
import time

spotter = KeywordSpotting()

# Debounce — ignore repeated triggers within 3 seconds
last_trigger_time = 0
DEBOUNCE_SECONDS = 3

def trigger(word):
    global last_trigger_time
    now = time.time()
    if now - last_trigger_time < DEBOUNCE_SECONDS:
        print(f"[SKIP] {word} ignored — too soon after last trigger")
        return
    last_trigger_time = now
    print(f"[TRIGGER] {word}")
    Bridge.call(word)

spotter.on_detect("Vivek", lambda: trigger("vivek"))
spotter.on_detect("Red",   lambda: trigger("red"))
spotter.on_detect("Blue",  lambda: trigger("blue"))
spotter.on_detect("Green", lambda: trigger("green"))

App.run()
