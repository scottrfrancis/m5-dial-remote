#include "FanView.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "DisplayManager.h"
#include "Graphics.h"

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

void FanView::onMqttConnected(PubSubClient& mqtt) {
  mqtt.subscribe(TOPIC_STATE);
}

bool FanView::onMqttMessage(const char* topic,
                            const uint8_t* payload,
                            unsigned int len) {
  if (strcmp(topic, TOPIC_STATE) != 0) {
    return false;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload, len);

  if (error) {
    Serial.print("FanView JSON parse failed: ");
    Serial.println(error.c_str());
    return true;  // topic was ours even if the payload was malformed
  }

  _speed = static_cast<uint8_t>(static_cast<int>(doc["speed"]));

  const char* dir = doc["direction"];
  if (dir) {
    _directionForward = (strcmp(dir, "forward") == 0);
  }

  _dirty = true;
  return true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void FanView::onActivate(DisplayManager& display) {
  _dirty = true;  // force full redraw when swiped to
}

void FanView::onDeactivate() {
  // nothing needed
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void FanView::onInput(const InputEvent& event, PubSubClient& mqtt) {
  switch (event.type) {
    case InputType::ButtonPress:
      // Toggle on/off. Off → on at medium speed; any speed → off.
      if (_speed > 0) {
        _speed = 0;
      } else {
        _speed = 3;
      }
      publishSpeed(mqtt);
      break;

    case InputType::EncoderDelta: {
      // Encoder produces 4 counts per physical detent; divide to get steps.
      int step     = event.delta / 4;
      int newSpeed = static_cast<int>(_speed) + step;
      _speed       = static_cast<uint8_t>(constrain(newSpeed, 0, 6));
      publishSpeed(mqtt);
      break;
    }

    case InputType::ButtonHold:
      // Toggle airflow direction.
      _directionForward = !_directionForward;
      publishDirection(mqtt);
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Update (called every loop on the active view)
// ---------------------------------------------------------------------------

void FanView::update(unsigned long now, DisplayManager& display) {
  if (_dirty) {
    drawFull(display);
    _dirty = false;
  }
}

// ---------------------------------------------------------------------------
// Private — MQTT publish helpers
// ---------------------------------------------------------------------------

void FanView::publishSpeed(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_speed"] = _speed;

  char payload[64];
  serializeJson(doc, payload);

  Serial.print("FanView: publishing speed ");
  Serial.println(_speed);

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("FanView: MQTT publish successful");
  } else {
    Serial.println("FanView: MQTT publish failed");
  }

  _dirty = true;
}

void FanView::publishDirection(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_direction"] = _directionForward ? "forward" : "reverse";

  char payload[64];
  serializeJson(doc, payload);

  Serial.print("FanView: publishing direction ");
  Serial.println(_directionForward ? "forward" : "reverse");

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("FanView: MQTT publish successful");
  } else {
    Serial.println("FanView: MQTT publish failed");
  }

  _dirty = true;
}

// ---------------------------------------------------------------------------
// Private — full repaint
// ---------------------------------------------------------------------------

void FanView::drawFull(DisplayManager& display) {
  display.clear();

  if (_speed == 0) {
    // Fan is off — show dim gray label
    display.drawCenteredText("OFF", 0x7BEF, 2.0f);
  } else {
    // Show speed label in cyan
    char buf[16];
    snprintf(buf, sizeof(buf), "Speed %d", _speed);
    display.drawCenteredText(buf, CYAN, 2.0f);

    // Speed arc — color indicates speed zone
    float normalized = _speed / 6.0f;

    uint16_t arcColor = (_speed <= 2) ? static_cast<uint16_t>(GREEN)
                      : (_speed <= 4) ? static_cast<uint16_t>(YELLOW)
                                      : static_cast<uint16_t>(CYAN);

    Gfx::drawValueArc(display.gfx(),
                      normalized,
                      arcColor,
                      Cfg::DISPLAY_CX,
                      Cfg::DISPLAY_CY,
                      Cfg::ARC_RADIUS,
                      Cfg::ARC_THICKNESS,
                      Cfg::ARC_START_DEG,
                      Cfg::ARC_SPAN_DEG);

    // Direction arrow — only shown when the fan is running
    Gfx::drawArrow(display.gfx(),
                   Cfg::DISPLAY_CX,
                   Cfg::DISPLAY_CY + 50,
                   _directionForward,
                   WHITE);
  }
}
