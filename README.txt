# Snapmaker U1 LED Visualizer

[![Version](https://img.shields.io/badge/version-11.57-blue.svg)](https://github.com/yourusername/snapmaker-led-visualizer)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-red.svg)](https://www.espressif.com/en/products/socs/esp32)

A sophisticated LED visualization system for Snapmaker U1 3D printers running Moonraker/Klipper firmware. This project transforms your printer's status into beautiful, customizable LED effects on a WS2812B/Neopixel LED strip.

## ✨ Features

### Core Functionality
- **Real-time Printer Monitoring** - Polls printer status every 500ms via Moonraker API
- **Intelligent State Machine** - Automatically detects and responds to printer states:
  - Idle, Heating, Printing, Paused, Finished, Error, Calibrating
- **Multi-extruder Support** - Monitors up to 4 extruders with individual color profiles
- **Temperature Tracking** - Real-time bed and extruder temperature display
- **Progress Visualization** - Visual progress bar with breathing effect LED strip

### LED Effects
- **7 Effect Types** per state:
  - Solid Color
  - Breathing/Pulsing
  - Blinking
  - Rainbow (for Finished state)
  - Wave effect (for Calibrating state)
  - Progress bar (for Printing state)
  - Tool identification flash

### Web Interface
- **Modern Responsive UI** - Works on desktop and mobile devices
- **Real-time Updates** - Live status, temperatures, and progress
- **Visual Effect Editor** - Configure colors, speeds, and effect types
- **5-Second Preview** - Test effects before saving
- **Tool Color Customization** - Assign unique colors to each extruder

### Advanced Features
- **Error Detection** - Anti-false error system with debouncing (15s error state)
- **Auto/Manual Mode** - Switch between automatic state tracking and manual override
- **Persistent Storage** - Saves all configurations to SPIFFS
- **WiFi Manager** - Easy network configuration through captive portal
- **Boot Animation** - Visual feedback during startup

## 📋 Requirements

### Hardware
- ESP32 Development Board (tested with ESP32-WROOM-32)
- WS2812B/Neopixel LED strip (21 LEDs - configurable)
- 5V power supply (adequate for your LED count)
- Snapmaker U1 with Moonraker/Klipper firmware

### Software
- Arduino IDE or PlatformIO
- Required Libraries:
  - WiFiManager by tzapu
  - ArduinoJson by Benoit Blanchon
  - Adafruit NeoPixel by Adafruit
  - WebServer (built-in)
  - SPIFFS (built-in)

## 🔧 Installation

### 1. Install Required Libraries

Using Arduino IDE Library Manager:
WiFiManager

ArduinoJson

Adafruit NeoPixel

text

### 2. Configure Printer IP

Edit the following line in the code:
```cpp
const char* printerIP = "192.168.1.54";   // CHANGE TO YOUR SNAPMAKER IP
3. Adjust LED Configuration (Optional)
cpp
#define NUM_LEDS 21      // Change to match your LED strip length
#define DATA_PIN 21      // Change to your ESP32 data pin
4. Upload to ESP32
Select your ESP32 board in Arduino IDE

Choose the correct COM port

Upload the sketch

5. Initial WiFi Setup
The ESP32 will create a WiFi access point named "SnapmakerLED"

Connect to this network from your phone/computer

Follow the captive portal to enter your WiFi credentials

🚀 Usage
Web Interface Access
Once connected to your network, access the web interface at:

text
http://[ESP32_IP_ADDRESS]
Find the ESP32 IP address in the Serial Monitor (115200 baud).

Manual Control Buttons
Button	Function
HEATING	Force heating state
PRINTING	Force printing state
PAUSE	Force paused state
FINISHED	Force finished state
ERROR	Force error state
IDLE	Force idle state
CALIBRATING	Force calibrating state
AUTO MODE	Return to automatic mode
IDENTIFY TOOL	Flash current tool's color
Effect Configuration
Scroll to the configuration panel

Select a state (HEATING, PRINTING, etc.)

Choose effect type and colors

Adjust speed using the slider

Click "PREVIEW" to test (5 seconds)

Click "SAVE" to store permanently

Click "RESET" to restore defaults

Tool Color Configuration
Find the "TOOL COLORS" panel

Use color pickers for each extruder (T0-T3)

Click "SAVE TOOL COLORS" to apply

🎨 LED Effect Types
Type	Description	Applicable States
Solid	Constant color	All except printing
Breathing	Smooth pulse effect	All except printing
Blinking	On/off flashing	All except printing
Rainbow	Cycling colors	Finished only
Wave	Moving gradient	Calibrating only
Progress Bar	Filling bar with breathing	Printing only
📊 State Machine Logic
text
IDLE → (print detected) → PRE-PRINTING (3s) → HEATING DELAY (3s) → HEATING → 
CALIBRATING → PRINTING → FINISHED → IDLE

Special transitions:
- PRINTING → PAUSED → PRINTING (resume)
- Any state → ERROR (15s) → IDLE
🔌 API Endpoints
The web server provides these endpoints for integration:

Endpoint	Method	Description
/api/status	GET	Current printer status as JSON
/force?state=X	GET	Force manual state
/auto	GET	Enable auto mode
/brightness?value=X	GET	Set brightness (0-255)
/identifyTool	GET	Flash current tool
/getToolColors	GET	Get tool colors as JSON
/saveToolColors	POST	Save tool colors
/getEffectsConfig	GET	Get all effect configurations
/saveEffect	GET	Save effect configuration
/resetEffect?state=X	GET	Reset effect to default
/previewEffect	GET	Preview effect for 5 seconds
🛠️ Troubleshooting
WiFi Connection Issues
Ensure ESP32 is within range of your router

Check that WiFi credentials were entered correctly

Monitor Serial output for connection status

No LED Response
Verify DATA_PIN matches your wiring

Check LED strip power supply (5V)

Confirm NUM_LEDS matches your strip length

Printer Not Detected
Verify printerIP is correct

Ensure Moonraker is running on port 7125

Test API access: http://[printer_ip]:7125/printer/objects/query?print_stats

Effects Not Saving
Check SPIFFS is properly initialized

Verify enough flash memory available

Monitor Serial for save errors

📁 File Structure
text
Snapmaker_LED_Visualizer/
├── Snapmaker_LED_Visualizer.ino   # Main sketch
├── data/
│   └── config.json                 # Auto-generated config file
├── README.md                       # This file
└── LICENSE                         # MIT License
🔧 Customization
Changing LED Count
cpp
#define NUM_LEDS 50  // Change to your LED count
Adjusting Update Interval
cpp
unsigned long updateInterval = 500;  // Milliseconds between API calls
Modifying Error Duration
cpp
const unsigned long ERROR_DURATION = 15000;  // 15 seconds
Adding New States
Add state name to stateNames[] array

Add label to stateLabels[] array

Add default effect to defaultEffects[]

Update state machine logic in updatePrinterStatusWithRetry()

📝 Version History
v11.57 (Current)
English language adaptation

Modern CSS without external fonts

Enhanced UI with neon effects and animations

Improved error handling with debouncing

Robust finished state management

5-second effect preview

Gradient buttons and pulsing status display

🤝 Contributing
Contributions are welcome! Please:

Fork the repository

Create a feature branch

Commit your changes

Push to the branch

Open a Pull Request

📧 Contact & Support
Author: Israel Garcia Armas with DeepSeek

Issues: GitHub Issues

Discussions: GitHub Discussions

⚠️ Disclaimer
This project is not affiliated with Snapmaker. Use at your own risk. Always monitor your printer during operation, especially when making configuration changes.

📄 License
This project is licensed under the MIT License - see the LICENSE file for details.

🙏 Acknowledgments
Snapmaker community for inspiration

Moonraker/Klipper developers for the excellent API

All contributors and testers

Made with ❤️ for the 3D printing community

text

This README provides:
- Complete project overview
- Installation instructions
- Usage guide for all features
- API documentation for developers
- Troubleshooting common issues
- Customization options
- Contribution guidelines

You can save this as `README.md` in your project folder. Adjust any URLs or contact information as needed for your specific setup.