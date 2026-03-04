#include "DashboardView.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "../Config.h"
#include "../core/DisplayManager.h"
#include "../gfx/Graphics.h"

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

void DashboardView::onMqttConnected(PubSubClient& mqtt) {
  mqtt.subscribe(TOPIC_TEMP);
  mqtt.subscribe(TOPIC_RECIRC);
  mqtt.subscribe(TOPIC_FAN);
}

bool DashboardView::onMqttMessage(const char* topic,
                                   const uint8_t* payload,
                                   unsigned int len) {
  StaticJsonDocument<128> doc;

  if (strcmp(topic, TOPIC_TEMP) == 0) {
    if (deserializeJson(doc, payload, len) == DeserializationError::Ok) {
      _tempF = doc["temp_degF"] | _tempF;
      _dirty = true;
    }
    return false;  // let WaterHeaterView also process this
  }

  if (strcmp(topic, TOPIC_RECIRC) == 0) {
    if (deserializeJson(doc, payload, len) == DeserializationError::Ok) {
      const char* pump = doc["pump"];
      if (pump) _pumpRunning = (strcmp(pump, "on") == 0);
      _dirty = true;
    }
    return false;  // let WaterHeaterView also process this
  }

  if (strcmp(topic, TOPIC_FAN) == 0) {
    if (deserializeJson(doc, payload, len) == DeserializationError::Ok) {
      _fanSpeed = static_cast<uint8_t>(static_cast<int>(doc["speed"]));
      const char* dir = doc["direction"];
      if (dir) {
        // Hubspace "forward" = downdraft → _fanForward = false
        _fanForward = (strcmp(dir, "reverse") == 0);
      }
      _dirty = true;
    }
    return false;  // let FanView also process this
  }

  return false;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void DashboardView::onActivate(DisplayManager& display) {
  _dirty      = true;
  _lastMinute = -1;
}

void DashboardView::onDeactivate() {
  _encoderAccum = 0;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void DashboardView::onInput(const InputEvent& event, PubSubClient& mqtt) {
  switch (event.type) {

    case InputType::EncoderDelta: {
      _encoderAccum += event.delta;
      int step = _encoderAccum / 4;
      if (step != 0) {
        _encoderAccum -= step * 4;
        int newSpeed = static_cast<int>(_fanSpeed) + step;
        _fanSpeed    = static_cast<uint8_t>(constrain(newSpeed, 0, 6));
        publishFanSpeed(mqtt);
      }
      break;
    }

    case InputType::TouchTap:
      _fanSpeed = (_fanSpeed > 0) ? 0 : 3;
      publishFanSpeed(mqtt);
      break;

    case InputType::ButtonPress:
      publishRecirc(mqtt);
      break;

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void DashboardView::update(unsigned long now, DisplayManager& display) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != _lastMinute) {
      _lastMinute = timeinfo.tm_min;
      _dirty = true;
    }
  }

  if (_dirty) {
    drawFull(display);
    _dirty = false;
  }
}

// ---------------------------------------------------------------------------
// Private — publish helpers
// ---------------------------------------------------------------------------

void DashboardView::publishFanSpeed(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["set_speed"] = _fanSpeed;

  char payload[64];
  serializeJson(doc, payload);

  mqtt.publish(TOPIC_CMD_FAN, payload);
  _dirty = true;
}

void DashboardView::publishRecirc(PubSubClient& mqtt) {
  StaticJsonDocument<64> doc;
  doc["start"] = 5;

  char payload[64];
  serializeJson(doc, payload);

  mqtt.publish(TOPIC_CMD_WATER, payload);
}

// ---------------------------------------------------------------------------
// Private — drawing
// ---------------------------------------------------------------------------

void DashboardView::drawFull(DisplayManager& display) {
  display.clear();
  drawTempArc(display);
  drawTime(display);
  drawFanBars(display);
}

void DashboardView::drawTempArc(DisplayManager& display) {
  float clamped    = constrain(_tempF, Cfg::TEMP_MIN_F, Cfg::TEMP_MAX_F);
  float normalized = (clamped - Cfg::TEMP_MIN_F) / (Cfg::TEMP_MAX_F - Cfg::TEMP_MIN_F);

  uint16_t arcColor;
  if (_pumpRunning) {
    arcColor = CYAN;
  } else if (_tempF < 100.0f) {
    arcColor = BLUE;
  } else if (_tempF < 120.0f) {
    arcColor = YELLOW;
  } else {
    arcColor = RED;
  }

  Gfx::drawValueArc(display.gfx(),
                     normalized,
                     arcColor,
                     Cfg::DISPLAY_CX,
                     Cfg::DISPLAY_CY,
                     Cfg::ARC_RADIUS,
                     Cfg::ARC_THICKNESS);
}

void DashboardView::drawTime(DisplayManager& display) {
  struct tm timeinfo;
  char timeBuf[8];
  char dateBuf[12];

  if (getLocalTime(&timeinfo)) {
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
    strftime(dateBuf, sizeof(dateBuf), "%a %d", &timeinfo);
  } else {
    strncpy(timeBuf, "--:--", sizeof(timeBuf));
    strncpy(dateBuf, "---", sizeof(dateBuf));
  }

  auto& gfx = display.gfx();
  gfx.setTextDatum(middle_center);

  // Water temp — above date
  char tempBuf[8];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", _tempF);
  gfx.setTextColor(0x7BEF);
  gfx.setTextSize(0.75f);
  gfx.drawString(tempBuf, Cfg::DISPLAY_CX, 72);

  // Date — smaller, orange
  gfx.setTextColor(0xFD20);  // orange
  gfx.setTextSize(0.75f);
  gfx.drawString(dateBuf, Cfg::DISPLAY_CX, 95);

  // Time — large, green
  gfx.setTextColor(GREEN);
  gfx.setTextSize(2.0f);
  gfx.drawString(timeBuf, Cfg::DISPLAY_CX, 125);
}

void DashboardView::drawFanBars(DisplayManager& display) {
  auto& gfx = display.gfx();

  // Speed bars — short (7px tall), color indicates direction
  Gfx::drawSpeedBars(gfx,
                      _fanSpeed,
                      _fanForward,
                      Cfg::DISPLAY_CX,
                      160,
                      16, 7, 4);
}
