# SPDX-FileCopyrightText: Copyright (C) Electronic Cats
# SPDX-License-Identifier: MPL-2.0
# Modified and extended by Copyright (c) Vivek Verma - 2026

from arduino.app_utils import *
from arduino.app_bricks.keyword_spotting import KeywordSpotting
from datetime import datetime, timezone, timedelta
import time

# Set your local timezone offset here (Germany = UTC+2 in summer, UTC+1 in winter)
LOCAL_TZ = timezone(timedelta(hours=2))   # CEST — change to hours=1 for CET (winter)

def ts():
    return datetime.now(LOCAL_TZ).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
# Add this Bridge.provide for logging from Arduino
def on_arduino_log(msg):
    print(f"[{ts()}] [ARDUINO] {msg}", flush=True)

Bridge.provide("pylog", on_arduino_log)

spotter = KeywordSpotting()
last_time = 0
DEBOUNCE_SECONDS = 3

def trigger(word):
    global last_time
    now = time.time()
    if now - last_time < DEBOUNCE_SECONDS:
        print(f"[{ts()}] [SKIP] {word} — too soon", flush=True)
        return
    last_time = now
    try:
        print(f"[{ts()}] [TRIGGER] {word}", flush=True)
        Bridge.call(word)
        print(f"[{ts()}] [SENT] {word} to Bridge", flush=True)
        time.sleep(0.5)
    except Exception as e:
        print(f"[{ts()}] [ERROR] {e}", flush=True)

spotter.on_detect("Vivek", lambda: trigger("vivek"))
spotter.on_detect("Red",   lambda: trigger("red"))
spotter.on_detect("Blue",  lambda: trigger("blue"))
spotter.on_detect("Green", lambda: trigger("green"))

print(f"[{ts()}] [INFO] Ready — say: vivek | red | blue | green", flush=True)

App.run()