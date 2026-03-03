#include "WaterHeaterView.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "Config.h"
#include "DisplayManager.h"
#include "Graphics.h"

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

void WaterHeaterView::onMqttConnected(PubSubClient& mqtt) {
  mqtt.subscribe(TOPIC_TEMP);
  mqtt.subscribe(TOPIC_RECIRC);
}

bool WaterHeaterView::onMqttMessage(const char* topic,
                                    const uint8_t* payload,
                                    unsigned int len) {
  if (strcmp(topic, TOPIC_TEMP) != 0 && strcmp(topic, TOPIC_RECIRC) != 0) {
    return false;
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload, len);

  if (error) {
    Serial.print("WaterHeaterView JSON parse failed: ");
    Serial.println(error.c_str());
    return true;  // topic was ours even if the payload was malformed
  }

  if (strcmp(topic, TOPIC_TEMP) == 0) {
    _tempF = doc["temp_degF"];
    _dirty = true;
    return true;
  }

  if (strcmp(topic, TOPIC_RECIRC) == 0) {
    const char* pumpState = doc["pump"];
    if (pumpState && strcmp(pumpState, "on") == 0) {
      if (!_pumpRunning) {
        _wheelAngle    = 0;
        _lastFrameTime = millis();
      }
      _pumpRunning = true;
    } else {
      _pumpRunning = false;
    }
    _dirty = true;
    return true;
  }

  return true;  // unreachable, but satisfies the compiler
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void WaterHeaterView::onActivate(DisplayManager& display) {
  _dirty = true;  // force full redraw when swiped to
}

void WaterHeaterView::onDeactivate() {
  // nothing needed
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void WaterHeaterView::onInput(const InputEvent& event, PubSubClient& mqtt) {
  if (event.type != InputType::ButtonPress) {
    return;
  }

  Serial.println("WaterHeaterView: button pressed — sending start command");

  StaticJsonDocument<64> doc;
  doc["start"] = 5;

  char payload[64];
  serializeJson(doc, payload);

  if (mqtt.publish(TOPIC_CMD, payload)) {
    Serial.println("WaterHeaterView: MQTT publish successful");
  } else {
    Serial.println("WaterHeaterView: MQTT publish failed");
  }
}

// ---------------------------------------------------------------------------
// Update (called every loop on the active view)
// ---------------------------------------------------------------------------

void WaterHeaterView::update(unsigned long now, DisplayManager& display) {
  // Full redraw when state has changed
  if (_dirty) {
    drawFull(display);
    _dirty           = false;
    _lastTempF       = _tempF;
    _lastPumpRunning = _pumpRunning;
  }

  // Animation tick: advance the color wheel without clearing the display
  // (the wheel draws over itself, so no full repaint is needed — avoids flicker)
  if (_pumpRunning && (now - _lastFrameTime > Cfg::FRAME_INTERVAL_MS)) {
    _wheelAngle    = (_wheelAngle + Cfg::CW_ADVANCE_DEG) % 360;
    _lastFrameTime = now;
    Gfx::drawColorWheel(display.gfx(),
                        _wheelAngle,
                        Cfg::DISPLAY_CX,
                        Cfg::DISPLAY_CY,
                        Cfg::CW_INNER_RADIUS,
                        Cfg::CW_OUTER_RADIUS);
  }
}

// ---------------------------------------------------------------------------
// Private — full repaint
// ---------------------------------------------------------------------------

void WaterHeaterView::drawFull(DisplayManager& display) {
  display.clear();

  // Temperature label — integer degrees, centered, large green text
  char buf[16];
  int tempWhole = static_cast<int>(round(_tempF));
  snprintf(buf, sizeof(buf), "%d F", tempWhole);
  display.drawCenteredText(buf, GREEN, 2.0f);

  // Temperature arc — normalize to [0, 1] over [TEMP_MIN_F, TEMP_MAX_F]
  float clamped  = constrain(_tempF, Cfg::TEMP_MIN_F, Cfg::TEMP_MAX_F);
  float normalized = (clamped - Cfg::TEMP_MIN_F) / (Cfg::TEMP_MAX_F - Cfg::TEMP_MIN_F);

  uint16_t arcColor = (_tempF < 100.0f) ? static_cast<uint16_t>(BLUE)
                    : (_tempF < 120.0f) ? static_cast<uint16_t>(YELLOW)
                                        : static_cast<uint16_t>(RED);

  Gfx::drawValueArc(display.gfx(),
                    normalized,
                    arcColor,
                    Cfg::DISPLAY_CX,
                    Cfg::DISPLAY_CY,
                    Cfg::ARC_RADIUS,
                    Cfg::ARC_THICKNESS);

  // Color wheel — only drawn when the recirculation pump is running
  if (_pumpRunning) {
    Gfx::drawColorWheel(display.gfx(),
                        _wheelAngle,
                        Cfg::DISPLAY_CX,
                        Cfg::DISPLAY_CY,
                        Cfg::CW_INNER_RADIUS,
                        Cfg::CW_OUTER_RADIUS);
  }
}
