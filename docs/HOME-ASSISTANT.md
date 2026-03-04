# Home Assistant Integration Guide

The M5Stack Dial communicates with Home Assistant indirectly through an MQTT broker. HA automations bridge entity state changes to MQTT topics (and vice versa). The Dial never talks to HA directly.

```text
Device  <-->  HA Integration  <-->  Home Assistant  <-->  EMQX Broker  <-->  M5Stack Dial
```

## Quick-Start Checklist

Follow these steps in order to add a new device to the Dial:

1. **Verify the HA integration** — confirm the device entity exists in **Developer Tools > States** and note its entity ID and attributes
2. **Create the state-publishing automation** — HA pushes entity state to an MQTT topic whenever it changes
3. **Create the command-listening automation** — HA acts on MQTT messages from the Dial
4. **Test with mosquitto CLI** — publish/subscribe manually to verify the automations work before involving the Dial (see [Testing](#testing-with-mosquitto-cli))
5. **Flash the Dial** — compile, upload, and verify serial output shows MQTT subscriptions

## MQTT Broker Setup (EMQX)

This project uses a standalone EMQX broker (not the Mosquitto HA add-on). Any MQTT 3.1.1+ broker works.

### Install EMQX

See [EMQX Getting Started](https://www.emqx.io/docs/en/latest/getting-started/getting-started.html) for your platform. Common options:

- **Docker:** `docker run -d --name emqx -p 1883:1883 -p 18083:18083 emqx/emqx:latest`
- **Linux package:** Follow EMQX docs for apt/yum

The default listener is port 1883 (no auth). For a home network behind a firewall this is fine. For production, configure authentication.

### Connect Home Assistant to the Broker

1. Go to **Settings > Devices & Services > Add Integration > MQTT**
2. Enter broker IP and port (1883)
3. Leave username/password blank if EMQX has no auth configured
4. Test the connection and submit

### Configure the Dial

Copy the broker's IP into your `environment.h`:

```cpp
const char *mqtt_server = "192.168.1.100";  // your EMQX broker IP
```

Do not use mDNS hostnames (e.g. `mqtt.local`) — they are unreliable on ESP32-S3.

## Automation Pattern

Each device needs two automations:

1. **State publisher** — triggers on HA entity state change, publishes JSON to an MQTT topic
2. **Command listener** — triggers on MQTT message, calls HA service

```text
Entity state change  -->  automation  -->  mqtt.publish(topic, JSON)  -->  Dial subscribes
Dial publishes       -->  MQTT topic  -->  automation triggers         -->  HA service call
```

Use separate topics for state (HA -> Dial) and commands (Dial -> HA) to avoid echo loops where the device receives its own publishes.

## Creating Automations in Home Assistant

There are two ways to add automations. Use whichever you prefer.

### Option A: YAML files (recommended for version control)

Your `automations.yaml` is a YAML list — each automation is a `- alias:` entry. To add new automations:

1. Open `automations.yaml` in your HA configuration directory (e.g. `/config/automations.yaml`)
2. Append the YAML blocks from the sections below to the end of the file — each block starts with `- alias:` and is already formatted as a list item
3. Go to **Developer Tools > YAML > Reload Automations** (or restart HA)
4. Verify the new automations appear under **Settings > Automations & Scenes**

HA will auto-assign an `id` field when it first loads each new automation.

### Option B: UI editor

1. Go to **Settings > Automations & Scenes > Create Automation > Create New Automation**
2. Click the three-dot menu (top right) and select **Edit in YAML**
3. HA shows a scaffold with empty `triggers`, `conditions`, and `actions`. **Select all** the scaffold text and **replace** it with the YAML block content — but strip the leading `- ` (the list dash). The UI expects a single automation object, not a list item.
4. Click **Save**
5. Repeat for each automation

Each YAML block below is formatted two ways:
- **For `automations.yaml`** (Option A): use as-is — each starts with `- alias:` as a list item
- **For the UI editor** (Option B): strip the leading `- ` so it starts with `alias:`

## Ceiling Fan Automations

Entity: `fan.ceiling_fan` (via [Hubspace integration](https://github.com/jdeath/Hubspace-Homeassistant))

> **Before you start:** Verify your entity IDs match. Go to **Developer Tools > States** in HA and search for your fan entity. The entity ID (e.g. `fan.ceiling_fan`) and its attributes (`percentage`, `direction`) must match the templates below. Adjust if your entity has a different name.

**Hubspace latency:** The Hubspace integration communicates with a cloud API, not local control. State changes from a physical remote follow the path: remote → fan → Hubspace cloud → HA → MQTT → Dial. Expect 15–30 seconds of delay for state updates from the physical remote. Commands sent from the Dial follow the reverse path and have similar latency. This is inherent to the cloud-based integration and not something the Dial firmware can improve.

### Publish Fan State to MQTT

Publishes speed and direction whenever the fan entity changes.

```yaml
- alias: MQTT - Fan State to Dial
  description: Publish bedroom fan speed and direction to MQTT for the M5 Dial
  mode: single
  triggers:
  - trigger: state
    entity_id:
      - fan.ceiling_fan
  conditions: []
  actions:
  - action: mqtt.publish
    metadata: {}
    data:
      evaluate_payload: true
      qos: "0"
      retain: true
      topic: fan/bedroom/state
      payload: '{ "speed": {{ (trigger.to_state.attributes.percentage | default(0) / 16.667) | round(0) | int }}, "direction": "{{ trigger.to_state.attributes.direction | default("forward") }}" }'
```

**Speed mapping:** Hubspace fans use percentage (0–100) in steps of 16.667. The Dial uses 6 discrete speeds (0–6). The template divides by 16.667 and rounds to convert: 0%→0, 17%→1, 33%→2, 50%→3, 67%→4, 83%→5, 100%→6.

### Listen for Fan Commands from MQTT

```yaml
- alias: MQTT - Dial Fan Speed Command
  description: Set bedroom fan speed when M5 Dial publishes a command
  mode: single
  triggers:
  - trigger: mqtt
    topic: fan/bedroom/command
  conditions:
  - condition: template
    value_template: '{{ trigger.payload_json.set_speed is defined }}'
  actions:
  - choose:
    - conditions:
      - condition: template
        value_template: '{{ trigger.payload_json.set_speed == 0 }}'
      sequence:
      - action: fan.turn_off
        metadata: {}
        target:
          entity_id: fan.ceiling_fan
    default:
    - action: fan.set_percentage
      metadata: {}
      target:
        entity_id: fan.ceiling_fan
      data:
        percentage: '{{ (trigger.payload_json.set_speed * 16.667) | int }}'
```

```yaml
- alias: MQTT - Dial Fan Direction Command
  description: Set bedroom fan direction when M5 Dial publishes a command
  mode: single
  triggers:
  - trigger: mqtt
    topic: fan/bedroom/command
  conditions:
  - condition: template
    value_template: '{{ trigger.payload_json.set_direction is defined }}'
  actions:
  - action: fan.set_direction
    metadata: {}
    target:
      entity_id: fan.ceiling_fan
    data:
      direction: '{{ trigger.payload_json.set_direction }}'
```

> **Direction inversion:** The Dial firmware inverts the Hubspace direction convention. When the Dial sends `"set_direction": "forward"`, it means the user selected downdraft — which is what Hubspace calls "forward". No extra mapping is needed in the automation; the firmware handles the translation.

## Ceiling Light Automations

Entity: `light.ceiling_light_2` (same Hubspace integration, same physical device as the fan)

> **Verify entity:** Check **Developer Tools > States** for `light.ceiling_light_2`. Confirm it has `brightness` and `color_temp` attributes (mireds range: 200–370 for Hubspace lights). If your light doesn't support color temperature, remove the `color_temp` lines from the state publisher and skip the color temp command automation.

### Publish Light State to MQTT

```yaml
- alias: MQTT - Light State to Dial
  description: Publish bedroom light state, brightness, and color temp to MQTT
  mode: single
  triggers:
  - platform: state
    entity_id: light.ceiling_light_2
  conditions: []
  actions:
  - action: mqtt.publish
    metadata: {}
    data:
      evaluate_payload: true
      qos: "0"
      retain: true
      topic: fan/bedroom/light
      payload: '{ "state": "{{ states("light.ceiling_light_2") }}", "brightness": {{ state_attr("light.ceiling_light_2", "brightness") | default(0, true) }}, "color_temp": {{ state_attr("light.ceiling_light_2", "color_temp") | default(350, true) }} }'
```

### Listen for Light Commands from MQTT

```yaml
- alias: MQTT - Dial Light Toggle Command
  description: Toggle bedroom light when M5 Dial publishes a command
  mode: single
  triggers:
  - platform: mqtt
    topic: light/bedroom/command
  conditions:
  - condition: template
    value_template: '{{ trigger.payload_json.set_light is defined }}'
  actions:
  - choose:
    - conditions:
      - condition: template
        value_template: '{{ trigger.payload_json.set_light == ''off'' }}'
      sequence:
      - action: light.turn_off
        target:
          entity_id: light.ceiling_light_2
    default:
    - action: light.turn_on
      target:
        entity_id: light.ceiling_light_2
      data:
        brightness: '{{ trigger.payload_json.brightness | default(255) }}'
```

```yaml
- alias: MQTT - Dial Light Color Temp Command
  description: Set bedroom light color temperature from M5 Dial
  mode: single
  triggers:
  - platform: mqtt
    topic: light/bedroom/command
  conditions:
  - condition: template
    value_template: '{{ trigger.payload_json.set_color_temp is defined }}'
  actions:
  - action: light.turn_on
    target:
      entity_id: light.ceiling_light_2
    data:
      color_temp: '{{ trigger.payload_json.set_color_temp }}'
```

## Water Heater Automations

Entity: managed by [rinnai_control_r](https://github.com/explosivo22/rinnaicontrolr-ha) via local TCP.

These automations should already exist if the water heater view was working. Documented here for reference.

### Publish Temperature

```yaml
- alias: MQTT - Water Heater Temp to Dial
  description: Publish water heater outlet temperature to MQTT
  mode: single
  triggers:
  - platform: state
    entity_id: sensor.rinnai_outlet_temperature
  conditions: []
  actions:
  - action: mqtt.publish
    metadata: {}
    data:
      evaluate_payload: true
      qos: "0"
      retain: true
      topic: water/temp
      payload: '{ "temp_degF": {{ states("sensor.rinnai_outlet_temperature") }} }'
```

### Publish Pump State

```yaml
- alias: MQTT - Recirc Pump State to Dial
  description: Publish recirculation pump status to MQTT
  mode: single
  triggers:
  - platform: state
    entity_id: switch.rinnai_recirculation
  conditions: []
  actions:
  - action: mqtt.publish
    metadata: {}
    data:
      evaluate_payload: true
      qos: "0"
      retain: true
      topic: water/recirc
      payload: '{ "pump": "{{ states("switch.rinnai_recirculation") }}" }'
```

### Listen for Pump Start Command

Note: the command topic (`water/recirc/cmd`) is intentionally different from the state topic (`water/recirc`) to prevent the Dial from receiving its own publishes as state updates.

```yaml
- alias: MQTT - Dial Recirc Pump Command
  description: Start recirculation pump when M5 Dial sends a command
  mode: single
  triggers:
  - platform: mqtt
    topic: water/recirc/cmd
  conditions: []
  actions:
  - action: switch.turn_on
    target:
      entity_id: switch.rinnai_recirculation
  - delay:
      minutes: '{{ trigger.payload_json.start | default(5) }}'
  - action: switch.turn_off
    target:
      entity_id: switch.rinnai_recirculation
```

## Testing with mosquitto CLI

Verify automations work before flashing the Dial. Install `mosquitto-clients` (`brew install mosquitto` on macOS).

### Subscribe to all Dial topics

```sh
mosquitto_sub -h <broker-ip> -t "water/#" -t "fan/#" -v
```

### Simulate a fan state update (as if HA published it)

```sh
mosquitto_pub -h <broker-ip> -t "fan/bedroom/state" \
  -m '{"speed": 3, "direction": "forward"}'
```

### Simulate a Dial command (test that HA automation responds)

```sh
mosquitto_pub -h <broker-ip> -t "fan/bedroom/command" \
  -m '{"set_speed": 4}'
```

### Watch MQTT traffic in real time

```sh
mosquitto_sub -h <broker-ip> -t "#" -v
```

## Customizing Topic Names

Topic names are configurable in `environment.h`. See `environment.h.example` for the full list. Override any topic before including the firmware headers:

```cpp
#define TOPIC_FAN_STATE  "fan/living_room/state"
#define TOPIC_FAN_CMD    "fan/living_room/command"
```

When you change topic names on the Dial, update the corresponding HA automation YAML to match.
