# M5 Dial Remote

A multi-device IoT remote control built on the M5Stack Dial. Swipe between devices, twist the encoder to adjust values, press to act. Connects to Home Assistant via MQTT.

## Hardware

- **[M5Stack Dial](https://shop.m5stack.com/products/m5stack-dial-esp32-s3-smart-rotary-knob-w-1-28-round-touch-screen)** — ESP32-S3, 240x240 round LCD (GC9A01), capacitive touch, rotary encoder, button ([documentation](https://docs.m5stack.com/en/core/M5Dial))

## Device Views

| View | What it controls | Encoder | Button |
|------|-----------------|---------|--------|
| **Water Heater** | Temperature display + recirculation pump | — | Start pump |
| **Fan** | Ceiling fan speed and direction | Speed 0-6 | Toggle on/off |
| **Light** | Brightness and color temperature | Adjust brightness (hold screen for color temp) | Toggle on/off |
| **Settings** | Display brightness | Adjust brightness | — |

Swipe left/right to navigate between views. Page dots at the bottom show your position.

## MQTT Topics

| Device | Subscribe | Publish | Payload examples |
|--------|-----------|---------|------------------|
| Water Heater | `water/temp` | — | `{"temp_degF": 120.5}` |
| Water Heater | `water/recirc` | `water/recirc/cmd` | `{"pump": "on"}` / `{"start": 5}` |
| Fan | `fan/bedroom/state` | `fan/bedroom/command` | `{"speed": 3, "direction": "forward"}` |
| Light | `fan/bedroom/light` | `light/bedroom/command` | `{"brightness": 200, "color_temp": 300}` |

The Dial talks directly to an MQTT broker (EMQX). Home Assistant bridges entity state to MQTT via automations.

## Configuration

```sh
cp environment.h.example environment.h
# Edit environment.h with your WiFi credentials and MQTT broker IP
```

`environment.h` is gitignored. Never commit credentials.

## Building

```sh
arduino-cli core install m5stack:esp32
arduino-cli lib install "M5Dial" "PubSubClient" "ArduinoJson"
arduino-cli compile --fqbn m5stack:esp32:m5stack_dial .
arduino-cli upload --fqbn m5stack:esp32:m5stack_dial -p /dev/cu.usbmodem2101 .
```

## Project Structure

```
m5-dial-remote.ino              # Coordinator — setup() + loop()
environment.h                    # WiFi/MQTT credentials (gitignored)
src/
  Config.h                       # Compile-time constants
  InputEvent.h                   # Input event types
  DeviceView.h                   # Abstract base class for all views
  core/
    ConnectivityManager.h/.cpp   # WiFi + MQTT lifecycle
    DisplayManager.h/.cpp        # Display wrapper
    Navigator.h/.cpp             # Swipe navigation
  gfx/
    Graphics.h/.cpp              # Drawing primitives (arcs, wheels, arrows)
  views/
    WaterHeaterView.h/.cpp
    FanView.h/.cpp
    LightView.h/.cpp
    SettingsView.h/.cpp
docs/
  DESIGN.md                      # Architecture and design decisions
  IDEA.md                        # Product vision and UX spec
```

## Adding a New Device

1. Create `src/views/NewDeviceView.h/.cpp` implementing `DeviceView`
2. `#include` it in the `.ino`
3. Add a static instance and its pointer to the `views[]` array
4. No framework changes needed

See [docs/DESIGN.md](docs/DESIGN.md) for the full architecture.

## Dependencies

- **M5Dial** (includes M5Unified, M5GFX)
- **PubSubClient** — MQTT client
- **ArduinoJson** — JSON serialization

## License

MIT
