#  UNO Q Voice Matrix AI System

### Developed and Adapted by **Vivek Verma**

---

##  What This Project Does

This project turns your Arduino UNO Q into an **AI-powered interactive system** that connects:

*  **Voice commands (AI / Edge ML)**
*  **LED Matrix display (real-time feedback)**
*  **RGB LEDs (visual response)**
*  **Environmental sensing (BME680)**

 When a command is detected (like **“red”, “blue”, “green”, “vivek”**), the system:

1. Displays a corresponding **letter (R, B, G, V)** on the LED matrix
2. Blinks the associated **RGB LED**, then keeps it ON
3. Reads and prints **live environmental data** from the BME680 sensor

This creates a **complete edge-AI feedback loop**:

> Voice → AI model → Command → Hardware response → Sensor data

---

##  Supported Commands

| Command | Action                               |
| ------- | ------------------------------------ |
| `vivek` | Displays **V** + prints sensor data  |
| `red`   | Displays **R** + activates RED LED   |
| `blue`  | Displays **B** + activates BLUE LED  |
| `green` | Displays **G** + activates GREEN LED |

---

##  Attribution

This project is **adapted and extended** from:

 [https://github.com/ElectronicCats/Qualcomm-Arduino-Edge-WS2026](https://github.com/ElectronicCats/Qualcomm-Arduino-Edge-WS2026)

```bash
git clone https://github.com/vivek-vermaa/Qualcomm-Arduino-Edge-WS2026.git
cd Qualcomm-Arduino-Edge-WS2026/Software
chmod 777 setup-arduino-q-mic-applab.sh
run ./sudo Software/setup-arduino-q-mic-applab.sh
sudo reboot
Close all the windows and terminal and login again.

```

The original repository provides:

* UNO Q AI + Linux integration
* Bridge communication system
* Edge AI pipeline support

This project extends it with:

* Custom LED matrix rendering
* RGB interaction logic
* Sensor integration
* Personalized command system

---

##  Hardware Required

* Arduino UNO Q
* Onboard LED Matrix (12×8)
* 3 LEDs (Red, Blue, Green)
* BME680 sensor
* Microphone or Wecam (for AI voice input via Linux side). No additional I2S microohone or any embedded module needed for this project

---

##  Pin Configuration

| Component  | Pin |
| ---------- | --- |
| Red LED    | D2  |
| Blue LED   | D3  |
| Green LED  | D4  |
| BME680 SDA | A4  |
| BME680 SCL | A5  |

---

##  System Architecture

```
Voice Input (Webcam Mic)
        ↓
Edge AI Model (Edge Impulse)
        ↓
Python / App Layer
        ↓
Bridge (RouterBridge)
        ↓
Arduino Sketch
        ↓
LED Matrix + RGB + Sensor Output
```

---

##  How to Train the AI Model (Edge Impulse)

This is the **core of your system**.

### Step 1: Go to Edge Impulse

* Open: [https://edgeimpulse.com](https://edgeimpulse.com)
* Create a new project
* Choose:

  * **Audio (Keyword Spotting)**

---

### Step 2: Collect Data

Record multiple samples for each command:

* “vivek”
* “red”
* “blue”
* “green”

 Tips:

* Record **20–50 samples per word**
* Use different tones, speeds, and environments
* Add **background noise samples**

---

### Step 3: Label Data

Assign labels like:

* `vivek`
* `red`
* `blue`
* `green`

---

### Step 4: Create Impulse

* Processing block: **Audio (MFCC)**
* Learning block: **Classification (Neural Network)**

---

### Step 5: Train Model

* Click **Train**
* Ensure accuracy > **85%** for good performance

---

### Step 6: Deploy Model

Export as:

* **Linux (Python)** OR
* **TensorFlow Lite**

---

### Step 7: Connect to Arduino via Bridge

In your Python code:

```python
Bridge.call("red")
Bridge.call("blue")
Bridge.call("green")
Bridge.call("vivek")
```

 This triggers your Arduino functions:

```cpp
Bridge.provide("red", cmd_red);
```

---

##  How It Works (Code Overview)

### LED Matrix

* Uses a **12×8 pixel buffer**
* Letters are drawn manually using pixel mapping

### RGB LEDs

* Blink 3 times → stay ON

### BME680 Sensor

Prints:

* Temperature
* Humidity
* Pressure
* Gas resistance

### Bridge

* Connects AI model (Python/Linux) → Arduino sketch

---

## How to Run

1. Upload Arduino sketch
2. Connect hardware
3. Run AI model (Python / App Lab)
4. Speak command

---

##  Example Output

```
[CMD] RED — showing R
========= BME680 =========
  Temperature : 25.3 C
  Humidity    : 48.2 %
  Pressure    : 1013.25 hPa
  Gas         : 12.45 KOhms
==========================
```

---

##  Why This Project is Powerful

* Runs **AI locally (Edge AI)**
* No cloud dependency
* Real-time response
* Combines **AI + Embedded + Sensors + UI**

 This is a **complete edge intelligence system**, not just a demo.

---

##  Future Improvements

* Add gesture recognition (MediaPipe)
* Add camera-based object detection
* Display live sensor graphs
* Add mobile/web dashboard
* Multi-language voice support

---

##  License

MPL-2.0 (as per source code)

---

##  Author

**Vivek Verma <tovivekverma@hotmail.com>**

---

## Credits

* Electronic Cats — original UNO Q Edge AI setup
* Qualcomm-Arduino-Edge-WS2026 repository — base implementation
* Extended, customized, and integrated by Vivek Verma
