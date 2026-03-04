#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "../DeviceView.h"
#include "../core/DisplayManager.h"
#include "../InputEvent.h"
#include "../MqttTopics.h"

// ---------------------------------------------------------------------------
// LightView — controls a bedroom ceiling light via MQTT through Home Assistant.
//
// The rotary encoder is modal:
//   - Default mode:       encoder adjusts brightness (0–255)
//   - Touch-hold to enter color temp mode; tap to exit back to brightness
//   - Color temp mode:    encoder adjusts color temperature (200–370 mireds)
//
// Topics:
//   Subscribe: fan/bedroom/light       — {"state":"on","brightness":200,"color_temp":350}
//   Publish:   light/bedroom/command  — {"set_light":"on"/"off"} or
//                                       {"set_light":"on","brightness":N} or
//                                       {"set_color_temp":N}
// ---------------------------------------------------------------------------

class LightView : public DeviceView {
public:
  const char* name() const override { return "Light"; }

  void onMqttConnected(PubSubClient& mqtt) override;
  bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) override;
  void onActivate(DisplayManager& display) override;
  void onDeactivate() override;
  void onInput(const InputEvent& event, PubSubClient& mqtt) override;
  void update(unsigned long now, DisplayManager& display) override;

private:
  // MQTT topics (defaults in MqttTopics.h, overridable in environment.h)
  static constexpr const char* TOPIC_STATE = TOPIC_LIGHT_STATE;
  static constexpr const char* TOPIC_CMD   = TOPIC_LIGHT_CMD;

  // Device state (updated by MQTT, even when inactive)
  bool    _on         = false;
  uint8_t _brightness = 128;   // 0–255
  int16_t _colorTemp  = 333;   // 200–370 mireds (200=cool/5000K, 370=warm/2700K)

  // Encoder accumulator (same pattern as FanView)
  int16_t _encoderAccum = 0;

  // Modal encoder state
  enum class Mode : uint8_t { Brightness, ColorTemp };
  Mode _mode = Mode::Brightness;

  // Publish throttle — prevent MQTT flooding during fast encoder turns
  static constexpr unsigned long PUBLISH_INTERVAL_MS = 150;
  unsigned long _lastPublishMs = 0;

  // Rendering state
  bool _dirty = true;

  void drawFull(DisplayManager& display);
  void publishBrightness(PubSubClient& mqtt);
  void publishColorTemp(PubSubClient& mqtt);
  void publishToggle(PubSubClient& mqtt);
};
