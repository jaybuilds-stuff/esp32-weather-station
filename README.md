# Weather Station

An ESP32 indoor weather station with a DHT11 sensor, SSD1306 OLED display, and local web dashboard.

## Setup

1. Install the ESP32 board package in Arduino IDE.
2. Install **Adafruit GFX**, **Adafruit SSD1306**, **DHT sensor library**, and **Adafruit Unified Sensor**.
3. Copy `secrets.example.h` to `secrets.h` **in the same folder as `Weather_Station.ino`**.
4. Edit `secrets.h` to set `WIFI_SSID` and `WIFI_PASSWORD` to your own Wi-Fi credentials.
5. Select your ESP32 board and serial port, then upload the sketch.
6. Open Serial Monitor at 115200 baud to find the device IP address. Open that IP address in a browser on the same network.

## Wiring

| Device | ESP32 pin |
|---|---|
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| DHT11 data | GPIO 4 |

Connect power and ground according to your specific modules.

## Security

**Never commit `secrets.h` or your actual Wi-Fi password.** This repository includes `secrets.example.h` as a safe template. The `.gitignore` prevents Git from adding `secrets.h` when it is untracked, but it **cannot remove secrets from earlier commits**. If you have already committed credentials, remove them from Git history and change the password.

The web dashboard has no authentication and is intended only for a trusted local network. Do not expose it to the public internet.

## Notes

This measures indoor temperature and humidity, not outdoor weather forecasts. Readings held in RAM are lost on restart. DHT11 measurements have limited accuracy. 

