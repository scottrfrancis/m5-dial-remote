#include "LightView.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../Config.h"
#include "../core/DisplayManager.h"
#include "../gfx/Graphics.h"

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

void LightView::onMqttConnected(PubSubClient& mqtt) {
  mqtt.subscribe(TOPIC_STATE);
}

bool LightView::onMqttMessage(const char* topic,
                              const uint8_t* payload,
                              unsigned int len) {
  if (strcmp(topic, TOPIC_STATE) != 0) {
    return false;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload, len);

  if (error) {
    Serial.print("LightView JSON parse failed: ");
    Serial.println(error.c_str());
    return true;  // topic was ours even if the payload was malformed
  }

  const char* state = doc["state"];
  if (state) {
    _on = (strcmp(state, "on") == 0);
  }

  if (doc.containsKey("brightness")) {
    _brightness = static_cast<uint8_t>(doc["brightness"].as<int>());
  }

  if (doc.containsKey("color_temp")) {
    _colorTemp = static_cast<int16_t>(doc["color_temp"].as<int>());
  }

  _dirty = true;
  return true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void LightView::onActivate(DisplayManager& display) {
  _mode  = Mode::Brightness;
  _dirty = true;
}

void LightView::onDeactivate() {
  _mode = Mode::Brightness;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void LightView::onInput(const InputEvent& event, PubSubClient& mqtt) {
  switch (event.type) {

    case InputType::ButtonPress:
      _on = !_on;  // optimistic local update
      publishToggle(mqtt);
      _dirty = true;
      break;

    case InputType::EncoderDelta:
      if (!_on) break;  // ignore encoder when light is off
      _encoderAccum += event.delta;
      {
        int step = _encoderAccum / 4;  // one physical detent = 4 counts
        if (step != 0) {
          _encoderAccum -= step * 4;
          if (_mode == Mode::Brightness) {
            int adjusted = static_cast<int>(_brightness) + step * 16;
            _brightness  = static_cast<uint8_t>(constrain(adjusted, 1, 255));
            if (millis() - _lastPublishMs > PUBLISH_INTERVAL_MS) {
              publishBrightness(mqtt);
              _lastPublishMs = millis();
            }
          } else {
            int adjusted = static_cast<int>(_colorTemp) + step * 10;
            _colorTemp   = static_cast<int16_t>(constrain(adjusted, 200, 370));
            if (millis() - _lastPublishMs > PUBLISH_INTERVAL_MS) {
              publishColorTemp(mqtt);
              _lastPublishMs = millis();
            }
          }
          _dirty = true;
        }
      }
      break;

    case InputType::TouchHoldStart:
      // Hold enters color temp mode (sticky — stays until tapped)
      _mode  = Mode::ColorTemp;
      _dirty = true;
      break;

    case InputType::TouchHoldEnd:
      // Ignored — mode is sticky, exits via tap
      break;

    case InputType::TouchTap:
      // Tap exits color temp mode back to brightness
      if (_mode == Mode::ColorTemp) {
        _mode  = Mode::Brightness;
        _dirty = true;
      }
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Update (called every loop on the active view)
// ---------------------------------------------------------------------------

void LightView::update(unsigned long now, DisplayManager& display) {
  if (_dirty) {
    drawFull(display);
    _dirty = false;
  }
}

// ---------------------------------------------------------------------------
// Private — MQTT publish helpers
// ---------------------------------------------------------------------------

void LightView::publishToggle(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_light"] = _on ? "off" : "on";

  char payload[64];
  serializeJson(doc, payload);

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("LightView: toggle published");
  } else {
    Serial.println("LightView: toggle publish failed");
  }
}

void LightView::publishBrightness(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_light"]  = "on";
  doc["brightness"] = _brightness;

  char payload[64];
  serializeJson(doc, payload);

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("LightView: brightness published");
  } else {
    Serial.println("LightView: brightness publish failed");
  }
}

void LightView::publishColorTemp(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_color_temp"] = _colorTemp;

  char payload[64];
  serializeJson(doc, payload);

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("LightView: color temp published");
  } else {
    Serial.println("LightView: color temp publish failed");
  }
}

// ---------------------------------------------------------------------------
// Private — full repaint
// ---------------------------------------------------------------------------

void LightView::drawFull(DisplayManager& display) {
  display.clear();
  auto& gfx = display.gfx();

  // --- Light bulb icon (centered, slightly above midpoint) ---
  const int bx = Cfg::DISPLAY_CX;
  const int by = Cfg::DISPLAY_CY - 15;
  const int bulbR = 24;
  const uint16_t baseColor = 0x6B4D;  // dark silver screw cap

  if (!_on) {
    // Grey bulb + "OFF" label
    const uint16_t grey = 0x7BEF;
    gfx.fillCircle(bx, by, bulbR, grey);
    gfx.fillRoundRect(bx - 8, by + bulbR, 16, 12, 2, baseColor);

    gfx.setTextDatum(middle_center);
    gfx.setTextSize(1.0f);
    gfx.setTextColor(grey);
    gfx.drawString("OFF", bx, by + bulbR + 28);
    return;
  }

  // --- ON state ---
  // Color for arc and value label based on color temperature
  uint16_t tempColor = (_colorTemp < 326) ? static_cast<uint16_t>(0x5DDF)
                                          : static_cast<uint16_t>(0xFD20);

  // Yellow bulb + screw base
  gfx.fillCircle(bx, by, bulbR, YELLOW);
  gfx.fillRoundRect(bx - 8, by + bulbR, 16, 12, 2, baseColor);

  gfx.setTextDatum(middle_center);

  if (_mode == Mode::Brightness) {
    // Brightness percentage below bulb
    char buf[8];
    int pct = static_cast<int>(round(_brightness / 255.0f * 100.0f));
    snprintf(buf, sizeof(buf), "%d%%", pct);
    gfx.setTextSize(1.0f);
    gfx.setTextColor(WHITE);
    gfx.drawString(buf, bx, by + bulbR + 28);

    // Brightness arc
    float normalized = _brightness / 255.0f;
    Gfx::drawValueArc(gfx, normalized, YELLOW,
                      Cfg::DISPLAY_CX, Cfg::DISPLAY_CY,
                      Cfg::ARC_RADIUS, Cfg::ARC_THICKNESS);

    // Mode label
    gfx.setTextSize(0.6f);
    gfx.setTextColor(0x7BEF);
    gfx.drawString("BRIGHTNESS", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY + 45);

  } else {
    // Color temp in mireds below bulb
    char buf[8];
    snprintf(buf, sizeof(buf), "%dm", static_cast<int>(_colorTemp));
    gfx.setTextSize(1.0f);
    gfx.setTextColor(tempColor);
    gfx.drawString(buf, bx, by + bulbR + 28);

    // Color temperature arc
    float normalized = (_colorTemp - 200.0f) / (370.0f - 200.0f);
    Gfx::drawValueArc(gfx, normalized, tempColor,
                      Cfg::DISPLAY_CX, Cfg::DISPLAY_CY,
                      Cfg::ARC_RADIUS, Cfg::ARC_THICKNESS);

    // Mode label
    gfx.setTextSize(0.6f);
    gfx.setTextColor(0x7BEF);
    gfx.drawString("COLOR TEMP", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY + 45);
  }
}
