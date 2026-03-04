#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "../DeviceView.h"
#include "../core/DisplayManager.h"
#include "../InputEvent.h"
#include "../MqttTopics.h"

// ---------------------------------------------------------------------------
// FanView — controls a bedroom ceiling fan via MQTT through Home Assistant.
//
// Topics:
//   Subscribe: fan/bedroom/state   — {"speed": 3, "direction": "forward"}
//   Publish:   fan/bedroom/command — {"set_speed": 3}
//                                  — {"set_direction": "forward"}
//
// Speed range: 0 (off) through 6. Direction: "forward" or "reverse".
// Button press toggles on/off (default on speed is 3).
// Encoder adjusts speed. Button hold toggles direction.
// ---------------------------------------------------------------------------

class FanView : public DeviceView {
public:
  const char* name() const override { return "Fan"; }

  void onMqttConnected(PubSubClient& mqtt) override;
  bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) override;
  void onActivate(DisplayManager& display) override;
  void onDeactivate() override;
  void onInput(const InputEvent& event, PubSubClient& mqtt) override;
  void update(unsigned long now, DisplayManager& display) override;

private:
  // MQTT topics (defaults in MqttTopics.h, overridable in environment.h)
  static constexpr const char* TOPIC_STATE = TOPIC_FAN_STATE;
  static constexpr const char* TOPIC_CMD   = TOPIC_FAN_CMD;

  // Device state (updated by MQTT, even when inactive)
  uint8_t _speed            = 0;     // 0=off, 1-6
  bool    _directionForward = true;  // true=forward, false=reverse

  // Encoder accumulator — M5Dial encoder produces 4 counts per physical detent.
  // At high loop rates, readAndReset() returns partial counts (1-3).
  // Accumulate until a full detent (±4) before changing speed.
  int16_t _encoderAccum = 0;

  // Rendering state
  bool _dirty = true;

  void drawFull(DisplayManager& display);
  void publishSpeed(PubSubClient& mqtt);
  void publishDirection(PubSubClient& mqtt);
};
