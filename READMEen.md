# 🔱 LED Visualizer for ELEGOO Neptune 4 Max

**Stable version: 1.3.5 (EN / ES / FR)**  
*Author: Israel Garcia Armas with DeepSeek*

![LED Visualizer Demo](images/demo.gif)

---

## 📖 Project Origin

This project is a natural evolution of a LED visualization system originally developed for my **Snapmaker U1** printer. The Snapmaker U1 came with a **closed, heavily modified Klipper firmware** from the factory, which prevented access to the standard Moonraker API. I **modified the original firmware and extended it with Klipper Extended** to open communication and access real-time printer data. That first version was a success.

Using that experience, I adapted the system for **standard Klipper** on my **ELEGOO Neptune 4 Max** (a large-format printer I already had in my workshop). The result is this fully functional, robust, and easily **portable system for any Klipper + Moonraker printer**.

---

## 🎯 Main Objectives

- **Real-time monitoring** of printer states: idle, heating, printing (with progress bar), paused, finished, error, and bed calibration.
- **Clean, responsive web interface** (mobile, tablet, PC) showing temperatures, progress, and allowing effect configuration.
- **Fully customizable LED effects** for each state (solid, breathing, blinking, rainbow, wave) with SPIFFS storage.
- **Automatic bed calibration detection** using extruder temperature (140°C) and axis movement.
- **Attractive boot animation** (two blue snakes meeting at the center + color flash per phase).

---

## 🧩 Components & Technologies

| Area | Component / Technology |
| :--- | :--- |
| **Microcontroller** | ESP32 (NodeMCU-32S or similar) |
| **LED Strip** | NeoPixel (WS2812B) – 21 LEDs (configurable) |
| **Printer Firmware** | Klipper + Moonraker (standard on Neptune 4 Max) |
| **Arduino Libraries** | WiFiManager, ArduinoJson, Adafruit_NeoPixel, WebServer, SPIFFS |
| **Development Environment** | Arduino IDE 2.x |
| **Communication** | HTTP (JSON over Moonraker) |
| **Web Interface** | HTML5, CSS3, JavaScript (fetch, dynamic DOM) |

---

## ✨ Key Features

### 📡 WiFiManager – Simple & Persistent WiFi Setup

The system uses **WiFiManager** for autonomous WiFi connection without hardcoding credentials.

- **First boot** (or no saved credentials): ESP32 creates an access point called `Neptune4-Lights` (no password). Connect to it, and a captive portal appears to select your home WiFi and enter the password.
- **Credential storage**: Saved in ESP32 flash memory. On subsequent boots, it connects automatically.
- **Failure handling**: If connection fails (e.g., password changed), it recreates the AP for reconfiguration.

### 🚀 4‑Phase Boot Animation

1. **WiFi** – blue snakes + blue flash on center LED.
2. **SPIFFS** – blue snakes + yellow flash.
3. **Moonraker** – blue snakes + magenta flash.
4. **Final success** – 4 green blinks on the entire strip.

### 🤖 Automatic Printer States

| State | Default LED Effect | Description |
| :--- | :--- | :--- |
| `idle` | gentle green breathing | Printer at rest, no file loaded. |
| `heating` | orange breathing | Heating bed or nozzle. |
| `printing` | blue progress bar with slow breathing (4s cycle) | Printing in progress, bar fills according to real percentage. |
| `paused` | yellow blinking | Print paused. |
| `finished` | rainbow (persists 2 minutes) | Print finished. Cancelable from web (IDLE or AUTO MODE buttons). |
| `error` | red blinking | Error or cancellation (`error` / `cancelled`). |
| `calibrating` | green/blue wave | Bed calibration automatically detected (nozzle at 140°C + axis movement). |

### 🔧 Intelligent Calibration Detection

- **Trigger**: nozzle target temperature reaches exactly 140°C (the value used during bed leveling).
- **Monitoring**: detects movement on X, Y, Z axes (threshold > 2 mm).
- **Exit conditions**:
  - No movement for **5 consecutive seconds**.
  - Nozzle target temperature changes from 140°C.
  - **Safety timeout** of 5 minutes.

### 🌐 Web Interface

- **Clean, high‑contrast design** (light text on dark background, no dark‑on‑dark issues).
- **Real‑time display**: current state, progress bar, filename, actual and target temperatures (nozzle and bed).
- **Brightness control** (0–255).
- **Manual mode buttons** to force any state; returns to auto mode after 5 seconds.
- **Effect configuration panel** (hidden by default, revealed by a button):
  - Drop‑down selector for effect type (solid, breathing, blinking, rainbow, wave).
  - Colour pickers (primary and secondary for wave or progress bar background).
  - Speed adjustment (ms per cycle).
  - Live preview, saving, and restore defaults.
- **Version info**: version number, "Beta" tag, author credits.

### 🔄 Persistent `finished` State

- After a print, `finished` remains active for **2 minutes**.
- Cancelable manually from the web with **"IDLE"** or **"AUTO MODE"** buttons.
- Automatically exits if a new print starts.

### 🛠️ Configuration Storage

- Custom effects are saved in SPIFFS (`/config.json`).
- After ESP32 restart, the last configuration is automatically restored.

---

## ✅ Advantages & Pros

- **No printer firmware modification needed** – works with standard Moonraker API.
- **Fully customizable** (colours, effects, speeds) from a simple web interface.
- **Real‑time responsiveness** (update every 500 ms).
- **Easy installation** – direct connection to ESP32 pins, no extra components.
- **Very low cost** (approx. 15–30 € in hardware).
- **Scalable** – number of LEDs adjustable by changing one constant.
- **Portable** – ESP32 can be powered from any USB port, including the printer’s.
- **Compatible** with any Klipper + Moonraker printer (not only Neptune 4 Max).

---

## 📦 Required Hardware & Estimated Cost

| Component | Approx. Cost |
| :--- | :--- |
| ESP32 (NodeMCU-32S) | 5–8 € |
| NeoPixel LED strip (21 LEDs, WS2812B) | 5–10 € |
| Cables & connectors | 1–2 € |
| 5V power supply (optional, if not USB‑powered) | 5–10 € |
| **Total** | **15–30 €** |

---

## 🔧 Important: Changing the Data Pin

The code uses **GPIO 21** as the default data pin for the NeoPixel strip (`DATA_PIN`). If you have connected your LED strip to a different pin, you must change this constant **before compiling**:

1. Open the code in Arduino IDE.
2. Find this line near the top:
   ```cpp
   #define DATA_PIN 21