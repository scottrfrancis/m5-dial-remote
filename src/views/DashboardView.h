#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <time.h>

#include "../DeviceView.h"
#include "../core/DisplayManager.h"
#include "../InputEvent.h"
#include "../MqttTopics.h"

// ---------------------------------------------------------------------------
// DashboardView — composite status screen showing time, water temp, and fan.
//
// Reads:  water/temp, water/recirc, fan/bedroom/state
// Writes: fan/bedroom/command (encoder + tap), water/recirc/cmd (button)
//
// IMPORTANT: onMqttMessage always returns false so FanView and
// WaterHeaterView also receive the same messages and track their own state.
// ---------------------------------------------------------------------------

class DashboardView : public DeviceView {
public:
  const char* name() const override { return "Dashboard"; }

  void onMqttConnected(PubSubClient& mqtt) override;
  bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) override;
  void onActivate(DisplayManager& display) override;
  void onDeactivate() override;
  void onInput(const InputEvent& event, PubSubClient& mqtt) override;
  void update(unsigned long now, DisplayManager& display) override;

private:
  // Topic aliases (from MqttTopics.h macros)
  static constexpr const char* TOPIC_TEMP     = TOPIC_WATER_TEMP;
  static constexpr const char* TOPIC_RECIRC   = TOPIC_WATER_RECIRC;
  static constexpr const char* TOPIC_CMD_WATER = TOPIC_WATER_CMD;
  static constexpr const char* TOPIC_FAN      = TOPIC_FAN_STATE;
  static constexpr const char* TOPIC_CMD_FAN  = TOPIC_FAN_CMD;

  // Device state (shadow — updated by MQTT even when inactive)
  float   _tempF       = 50.0f;
  bool    _pumpRunning = false;
  uint8_t _fanSpeed    = 0;       // 0=off, 1-6
  bool    _fanForward  = true;

  // Encoder accumulator (4 ticks per detent)
  int16_t _encoderAccum = 0;

  // Rendering state
  bool _dirty      = true;
  int  _lastMinute = -1;

  void drawFull(DisplayManager& display);
  void drawTempArc(DisplayManager& display);
  void drawTime(DisplayManager& display);
  void drawFanBars(DisplayManager& display);

  void publishFanSpeed(PubSubClient& mqtt);
  void publishRecirc(PubSubClient& mqtt);
};
