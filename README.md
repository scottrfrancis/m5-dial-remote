# Water Heater Remote

An M5Stack Dial-based IoT controller that monitors water heater temperature and controls a recirculation pump via MQTT.

## Hardware

- **M5Stack Dial** (ESP32-S3, 240x240 round LCD, rotary encoder, button)

## Features

- Real-time temperature display with color-coded arc indicator (90-140°F)
- Animated color wheel ring when recirculation pump is running
- One-button recirculation pump activation
- MQTT pub/sub for temperature updates and pump control

## Display

- **Temperature**: Large centered reading in °F
- **Arc indicator**: 270° arc mapped from 90°F to 140°F
  - Blue: below 100°F
  - Yellow: 100-120°F
  - Red: above 120°F
- **Color wheel**: Animated ring when pump is active

## MQTT Topics

| Topic | Direction | Payload | Description |
|-------|-----------|---------|-------------|
| `water/temp` | Subscribe | `{"temp_degF": 120.5}` | Temperature updates from sensor |
| `water/recirc` | Subscribe | `{"pump": "on"}` or `{"pump": "off"}` | Pump state notifications |
| `water/recirc` | Publish | `{"start": 5}` | Request pump start (Button A) |

## Configuration

Copy and edit `environment.h` with your network credentials:

```c
const char *ssid       = "YourSSID";
const char *password   = "YourPassword";
const char *mqtt_server = "your-broker.local";
```

This file is gitignored to keep credentials out of version control.

## Dependencies

Install via Arduino Library Manager or `arduino-cli lib install`:

- **M5Dial** (includes M5Unified, M5GFX)
- **PubSubClient** — MQTT client
- **ArduinoJson** — JSON serialization/deserialization

## Building

### Arduino IDE / VSCode Arduino Extension

1. Install the M5Stack board package (Board Manager URL: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`)
2. Select board: **M5Dial** (`m5stack:esp32:m5stack_dial`)
3. Install the libraries listed above
4. Open `dial-water-heater-remote.ino` and upload

### arduino-cli

```sh
arduino-cli core install m5stack:esp32
arduino-cli lib install "M5Dial" "PubSubClient" "ArduinoJson"
arduino-cli compile --fqbn m5stack:esp32:m5stack_dial .
arduino-cli upload --fqbn m5stack:esp32:m5stack_dial -p /dev/ttyACM0 .
```

## Controls

- **Button A** (front face press): Send recirculation pump start command
