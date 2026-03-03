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
      publishToggle(mqtt);
      break;

    case InputType::EncoderDelta:
      if (_mode == Mode::Brightness) {
        int adjusted = static_cast<int>(_brightness) + event.delta * 4;
        _brightness  = static_cast<uint8_t>(constrain(adjusted, 0, 255));
        publishBrightness(mqtt);
      } else {
        int adjusted = static_cast<int>(_colorTemp) + event.delta * 8;
        _colorTemp   = static_cast<int16_t>(constrain(adjusted, 153, 500));
        publishColorTemp(mqtt);
      }
      _dirty = true;
      break;

    case InputType::TouchHoldStart:
      _mode  = Mode::ColorTemp;
      _dirty = true;
      break;

    case InputType::TouchHoldEnd:
      _mode  = Mode::Brightness;
      _dirty = true;
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

  // Light is off — show a single dim label and nothing else
  if (!_on) {
    display.drawCenteredText("OFF", 0x7BEF, 2.0f);
    return;
  }

  // Color for the arc and value label.
  // Below the midpoint (326 mireds) = cool blue; at or above = warm orange.
  uint16_t tempColor = (_colorTemp < 326) ? static_cast<uint16_t>(0x5DDF)
                                          : static_cast<uint16_t>(0xFD20);

  if (_mode == Mode::Brightness) {
    // Main label — brightness as a percentage
    char buf[8];
    int pct = static_cast<int>(round(_brightness / 255.0f * 100.0f));
    snprintf(buf, sizeof(buf), "%d%%", pct);
    display.drawCenteredText(buf, WHITE, 2.0f);

    // Brightness arc
    float normalized = _brightness / 255.0f;
    Gfx::drawValueArc(display.gfx(),
                      normalized,
                      YELLOW,
                      Cfg::DISPLAY_CX,
                      Cfg::DISPLAY_CY,
                      Cfg::ARC_RADIUS,
                      Cfg::ARC_THICKNESS);

    // Mode label
    display.gfx().setTextDatum(middle_center);
    display.gfx().setTextSize(0.6f);
    display.gfx().setTextColor(0x7BEF);
    display.gfx().drawString("BRIGHTNESS", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY + 40);

  } else {
    // Main label — color temperature in mireds
    char buf[8];
    snprintf(buf, sizeof(buf), "%dm", static_cast<int>(_colorTemp));
    display.drawCenteredText(buf, tempColor, 2.0f);

    // Color temperature arc — normalized over the 153–500 mired range
    float normalized = (_colorTemp - 153.0f) / (500.0f - 153.0f);
    Gfx::drawValueArc(display.gfx(),
                      normalized,
                      tempColor,
                      Cfg::DISPLAY_CX,
                      Cfg::DISPLAY_CY,
                      Cfg::ARC_RADIUS,
                      Cfg::ARC_THICKNESS);

    // Mode label
    display.gfx().setTextDatum(middle_center);
    display.gfx().setTextSize(0.6f);
    display.gfx().setTextColor(0x7BEF);
    display.gfx().drawString("COLOR TEMP", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY + 40);
  }
}
