# Dial Remote — Project Vision

Use the M5Stack Dial as a handheld remote control for HomeAssistant-connected devices. Swipe between device views, twist the encoder to adjust values, press to act.

The general pattern: each device connects to Home Assistant through its own integration, HA bridges entity state to MQTT via automations, and the Dial subscribes/publishes directly to an EMQX broker. The Dial never talks to HA — only MQTT.

## M5Stack Dial Hardware

| Capability | Details |
| --- | --- |
| Display | 240x240 round IPS LCD (GC9A01), 1.28", 16-bit color |
| Touchscreen | Capacitive (FT5x06), up to 5 points |
| Touch gestures | Tap, hold, flick/swipe, drag — via M5Unified `Touch_Class` |
| Rotary encoder | Quadrature, interrupt-driven, 4 counts/detent, signed position |
| Button | Encoder shaft press (BtnA) — click, double-click, hold |
| Speaker | Piezo buzzer on GPIO 3 |
| RTC | BM8563 real-time clock |
| RFID | MFRC522 (optional, enabled at init) |
| Ambient light sensor | __None__ — brightness is manual via `setBrightness(0-255)` |
| IMU | None |
| WiFi | ESP32-S3, 2.4 GHz |

Key APIs for multi-device navigation:

- `M5Dial.Touch.getDetail()` → `wasFlicked()`, `distanceX()`, `distanceY()` for swipe detection
- `M5Dial.Encoder.read()` / `readAndReset()` for rotary input
- `M5Dial.BtnA.wasPressed()`, `wasDoubleClicked()`, `wasHold()` for button actions

## System Architecture

```text
┌──────────────────┐     local TCP      ┌────────────────────────────┐
│ Rinnai Water      │◄──────────────────►│ rinnai_control_r (HACS)    │
│ Heater            │     port 9798      │ by explosivo22             │
└──────────────────┘                     └─────────────┬──────────────┘
                                                       │
                                                       ▼
┌──────────────────┐     cloud API      ┌──────────────────────────┐
│ Hubspace Ceiling  │◄─────────────────►│ Home Assistant            │
│ Fan/Light         │                    │                          │
└──────────────────┘                     └─────────────┬────────────┘
                                                       │ automations
                                                       │ (state → MQTT,
                                                       │  MQTT → service calls)
                                                       ▼
                                         ┌──────────────────────────┐
                                         │ EMQX Broker (standalone) │
                                         └─────────────┬────────────┘
                                                       │
                                                       ▼
                                         ┌──────────────────────────┐
                                         │ M5Stack Dial              │
                                         │ (this project)           │
                                         └──────────────────────────┘
```

- **EMQX** runs standalone (not an HA add-on)
- **HA automations** bridge entity state changes bidirectionally to MQTT topics
- **M5Stack Dial** subscribes and publishes directly to EMQX — no HA dependency at runtime

## Device 1: Water Heater (working)

### Integration

[rinnai_control_r](https://github.com/explosivo22/rinnaicontrolr-ha) — HACS custom integration that communicates with the Rinnai water heater via local TCP (port 9798) through a WiFi module on the unit.

Available HA entities from this integration:

- Outlet temperature, inlet temperature
- Water flow rate (GPM)
- Heating status (active/inactive)
- Recirculation pump status (on/off)
- Combustion cycles, operation hours, pump hours/cycles
- Fan current and frequency

### MQTT Topics (bridged by HA)

| Topic | Direction | Payload | Purpose |
| --- | --- | --- | --- |
| `water/temp` | HA → Dial | `{"temp_degF": 120.5}` | Outlet temperature updates |
| `water/recirc` | HA → Dial | `{"pump": "on"}` or `{"pump": "off"}` | Pump state notifications |
| `water/recirc` | Dial → HA | `{"start": 5}` | Request pump start (5-min duration) |

### Display

- Center: large temperature reading in degrees F
- Arc indicator: 270° arc mapped from 90°F to 140°F
   - Blue below 100°F, yellow 100-120°F, red above 120°F

- Outer ring: animated color wheel when recirculation pump is running
- Button A press sends recirculation start command

### Status

Working and deployed. Non-blocking WiFi/MQTT resilience with exponential backoff added.

## Device 2: Bedroom Ceiling Fan/Light (planned)

### Integration

[Hubspace-Homeassistant](https://github.com/jdeath/Hubspace-Homeassistant) — HACS custom integration that communicates with Hubspace (Afero-based) devices via their cloud API.

Expected HA entities:

- `fan.bedroom_fan` — speed (off/low/medium/high), direction (forward/reverse), on/off
- `light.bedroom_light` — on/off, brightness (0-255), color temperature

### MQTT Topics (to be created)

These automations do not exist yet — they need to be built in HA.

| Topic | Direction | Payload | Purpose |
| --- | --- | --- | --- |
| `fan/bedroom/state` | HA → Dial | `{"speed": 3, "direction": "forward"}` | Fan state (speed 0-6, direction) |
| `fan/bedroom/light` | HA → Dial | `{"state": "on", "brightness": 200, "color_temp": 350}` | Light state updates |
| `fan/bedroom/command` | Dial → HA | `{"set_speed": 4}` | Set fan speed (0=off, 1-6) |
| `fan/bedroom/command` | Dial → HA | `{"set_direction": "reverse"}` | Set fan direction |
| `fan/bedroom/command` | Dial → HA | `{"set_light": "on", "brightness": 128}` | Light control |
| `fan/bedroom/command` | Dial → HA | `{"set_color_temp": 400}` | Light color temperature |

See [Navigation and UX](#navigation-and-ux) below for display and controls spec.

## HA Automation Setup

Each device needs a pair of HA automations to bridge between the integration's entities and MQTT:

**State → MQTT (HA publishes when entity changes):**

```yaml
# Example: fan state to MQTT
automation:
  trigger:
    - platform: state
      entity_id: fan.bedroom_fan
  action:
    - service: mqtt.publish
      data:
        topic: fan/bedroom/state
        payload_template: >
          {"speed": "{{ states('fan.bedroom_fan') }}",
           "direction": "{{ state_attr('fan.bedroom_fan', 'direction') }}"}
```

**MQTT → Service Call (HA acts on commands from Dial):**

```yaml
# Example: MQTT command to fan service
automation:
  trigger:
    - platform: mqtt
      topic: fan/bedroom/command
  action:
    - service: fan.set_speed
      target:
        entity_id: fan.bedroom_fan
      data:
        speed: "{{ trigger.payload_json.set_speed }}"
```

The water heater automations already exist and follow this same pattern.

## Settings View

A dedicated settings page accessible by swiping to the last position in the view array.

### Brightness Control

The M5Stack Dial has no ambient light sensor — display brightness must be set manually via `M5Dial.Display.setBrightness(0-255)`.

- **Rotary encoder**: Adjust brightness (0–255)
- **Display**: Brightness percentage centered, with a value arc showing current level
- Brightness persists in RAM for the session (resets to default on power cycle)

Future settings candidates: MQTT broker display, WiFi signal strength, device reorder, sleep timeout.

### MQTT Topics

None — settings are local to the device.

## Future Device Ideas

The pattern is always the same: HA integration + MQTT bridge automations + Dial view.

- Garage door (open/close status, trigger)
- Thermostat (current temp, setpoint adjustment)
- Security system (arm/disarm)
- Sprinkler zones

## Navigation and UX

Swipe left/right on the touchscreen to cycle between four views:

**Water Heater → Fan → Light → Settings**

Page indicator dots at the bottom of the screen show current position (like iOS).

Implementation: detect horizontal flick via `M5Dial.Touch.getDetail()` → `wasFlicked()` + `distanceX()`. Maintain a device index into an extensible view array.

### Water Heater View (existing)

- **Button press**: Start recirculation (5-min duration)
- **Display**: Temperature arc (90-140°F, color-coded) + color wheel animation when pump runs

### Fan View

- **Button press**: Toggle fan on/off
- **Rotary encoder**: Cycle through 6 speed levels (0=off, 1-6)
- **Tap+hold**: Toggle airflow direction (forward/reverse)
- **Display**:
  - Gauge-style arc (like the water heater temp arc) showing speed as percentage
  - Color-coded by speed range
  - Airflow direction indicator — clear up/down arrow showing current flow direction

### Light View

- **Button press**: Toggle light on/off
- **Rotary encoder**: Adjust brightness level
- **Tap+hold**: Enter color temperature adjustment mode — rotary to adjust, tap to accept/exit
- **Display**:
  - Brightness level arc or fill indicator
  - Color temperature indicator (warm/cool) when in adjustment mode

### Settings View

- **Rotary encoder**: Adjust display brightness (0–255)
- **Display**: Brightness percentage centered, value arc showing current level
- No MQTT interaction — all settings are local to the device
- Future candidates: MQTT broker info, WiFi signal strength, sleep timeout
