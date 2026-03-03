#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "../DeviceView.h"
#include "../core/DisplayManager.h"
#include "../InputEvent.h"

// ---------------------------------------------------------------------------
// WaterHeaterView — displays water heater temperature and recirculation pump
// state. Receives live data via MQTT and supports a button press to trigger
// a timed recirculation run.
//
// Topics:
//   Subscribe: water/temp      — {"temp_degF": 115.3}
//   Subscribe: water/recirc    — {"pump": "on"} or {"pump": "off"}
//   Publish:   water/recirc/cmd — {"start": 5}
//
// The publish topic is intentionally different from the subscribe topic so
// the device does not receive its own commands back as state updates.
// ---------------------------------------------------------------------------

class WaterHeaterView : public DeviceView {
public:
  const char* name() const override { return "Water"; }

  void onMqttConnected(PubSubClient& mqtt) override;
  bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) override;
  void onActivate(DisplayManager& display) override;
  void onDeactivate() override;
  void onInput(const InputEvent& event, PubSubClient& mqtt) override;
  void update(unsigned long now, DisplayManager& display) override;

private:
  // MQTT topics
  static constexpr const char* TOPIC_TEMP   = "water/temp";
  static constexpr const char* TOPIC_RECIRC = "water/recirc";
  static constexpr const char* TOPIC_CMD    = "water/recirc/cmd";  // separate pub topic (bug fix)

  // Device state (updated by MQTT, even when inactive)
  float _tempF       = 50.0f;
  bool  _pumpRunning = false;

  // Rendering state
  bool  _dirty           = true;
  float _lastTempF       = -999.0f;
  bool  _lastPumpRunning = false;
  int   _wheelAngle      = 0;
  unsigned long _lastFrameTime = 0;

  void drawFull(DisplayManager& display);
};
