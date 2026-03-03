#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "DeviceView.h"
#include "DisplayManager.h"
#include "InputEvent.h"

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
  // MQTT topics
  static constexpr const char* TOPIC_STATE = "fan/bedroom/state";
  static constexpr const char* TOPIC_CMD   = "fan/bedroom/command";

  // Device state (updated by MQTT, even when inactive)
  uint8_t _speed            = 0;     // 0=off, 1-6
  bool    _directionForward = true;  // true=forward, false=reverse

  // Rendering state
  bool _dirty = true;

  void drawFull(DisplayManager& display);
  void publishSpeed(PubSubClient& mqtt);
  void publishDirection(PubSubClient& mqtt);
};
