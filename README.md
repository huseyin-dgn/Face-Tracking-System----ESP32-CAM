# 📌 Face Tracking System with ESP32-CAM 📌

## 📖 Description

This project enables real-time face tracking using ESP32-CAM. Developed with OpenCV and C++, the system detects a face and adjusts the camera's direction accordingly. It provides a cost-effective and efficient solution for face tracking applications.

## 🛠️ Features

- 📷 Real-time face detection and tracking
- ⚡ Low-cost and efficient implementation
- 🎯 OpenCV-based face tracking algorithm

## 🏗️ Hardware Requirements

- ESP32-CAM module
- Power supply (5V)
- Computer with Arduino IDE
- FTDI Programmer (for flashing the code)
- Jumper wires

## 🔧 Software Requirements

- Arduino IDE
- OpenCV (for image processing)
- ESP32 Board Package (install via Arduino IDE)

## 🚀 Installation

### Clone the Repository

```bash
git clone https://github.com/yourusername/Face-Tracking-ESP32.git
cd Face-Tracking-ESP32
```

### Install the ESP32 Board in Arduino IDE

1. Open **Arduino IDE**
2. Go to **File** → **Preferences**
3. Add the following URL to **Additional Board Manager URLs**:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
4. Go to **Tools** → **Board** → **Board Manager**
5. Search for **ESP32** and install it

### Install Required Libraries

Open the Arduino IDE and install the following libraries:

```cpp
#include <WiFi.h>
#include <esp_camera.h>
```

### Wiring ESP32-CAM to FTDI Programmer

Connect the ESP32-CAM to the FTDI Programmer using the following wiring:

| ESP32-CAM Pin | FTDI Programmer Pin |
|--------------|-------------------|
| GND          | GND               |
| 5V           | VCC (5V)          |
| U0R (RX)     | TX                |
| U0T (TX)     | RX                |
| IO0          | GND (for flashing) |

**Note:** IO0 should be connected to GND only during flashing. Remove the connection after flashing.

### Flash the Code to ESP32-CAM

1. Connect ESP32-CAM to your FTDI programmer as described above
2. Select the correct **COM Port**
3. Click **Upload**

### Run the System

1. Power the ESP32-CAM
2. Connect to the serial monitor
3. The face tracking should start automatically

