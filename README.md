# ESP32-S3 Camera HTTP Stream Server (MJPEG)

A lightweight real-time camera stream server for ESP32-S3 devices using HTTP and WiFi SoftAP configuration with MJPEG video feed support over local network or localhost access point (default port 80). Designed to work on minimum N4R4 pinout-based boards, it provides a web interface to view live feeds without requiring dedicated hardware.

<p align="center">
  <img src="https://github.com/user-attachments/assets/093a059a-c035-4b90-95bd-c107a57da932"
       alt="520"
       width="350">
</p>

##  **Hardware Requirements**: ESP32-S3 Dev Module / Arduino Nano V3

```text
Use these Arduino IDE settings:

Setting	Value
Board	ESP32S3 Dev Module
PSRAM	OPI PSRAM
Flash Mode	QIO 80MHz
Flash Size	16MB
Partition	16M Flash / 3MB APP / 9.9MB FATFS
USB CDC On Boot	Enabled
```

## Project Features
- **Camera Stream**: MJPEG stream over HTTP server (`/stream`) with web UI support at the root folder for viewing video feed live or recording locally using `ESP-Nets`.
- **WiFi AP Configuration**: Configured to use SSID `"ESP32-CAM"` and password `"12345678"`, allowing external devices on local networks to connect.
- **PSRAM Optimization**: Detects PSRAM usage, dynamically adjusts frame size (VGA/QUVA/QVGA) for memory stability depending on `psramFound()`.

## Dependencies & Libraries
Ensure the following libraries are in your Arduino IDE installation:

```cpp
#include "esp_camera.h"  // ESP32 camera initialization
#include <WiFi.h>        // WiFi SoftAP configuration
#include "esp_http_server.h"  // HTTP server setup for /stream handler
```

## Hardware Pinout Configuration (ESP-S3N16R8)
- **Reset**: GPIO # -1 (Hardware Reset or software trigger if needed).
- **XCLK/PIN PWR**: GPIO # -1.
- **Data Lines** (D0-D7): Defined by pin mapping in `config.pin_*` for VGA/MP4 capture logic (`Y2` to `SIOC`).

## Installation & Configuration Steps
1. Install the latest version of `Arduino IDE`.
2. Configure your board using: Tools -> Board Settings (Select ESP32-S3 Dev Module).
3. Enable USB CDC on Boot in Device Menu Options.
4. Open the file with `tools > Serial Port` to see real-time stream output.

## Live feed
After connecting your PC/smartphone with wifi, open http://192.168.4.1/ in browser and check the live feed.
```
