#pragma once

#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "DeviceView.h"
#include "DisplayManager.h"
#include "InputEvent.h"

// ---------------------------------------------------------------------------
// LightView — controls a bedroom ceiling light via MQTT through Home Assistant.
//
// The rotary encoder is modal:
//   - Default mode:       encoder adjusts brightness (0–255)
//   - Touch-hold active:  encoder adjusts color temperature (153–500 mireds)
//
// Topics:
//   Subscribe: fan/bedroom/light   — {"state":"on","brightness":200,"color_temp":350}
//   Publish:   fan/bedroom/command — {"set_light":"on"/"off"} or
//                                    {"set_light":"on","brightness":N} or
//                                    {"set_color_temp":N}
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
  // MQTT topics
  static constexpr const char* TOPIC_STATE = "fan/bedroom/light";
  static constexpr const char* TOPIC_CMD   = "fan/bedroom/command";

  // Device state (updated by MQTT, even when inactive)
  bool    _on         = false;
  uint8_t _brightness = 128;   // 0–255
  int16_t _colorTemp  = 350;   // 153–500 mireds (153=cool/6500K, 500=warm/2000K)

  // Modal encoder state
  enum class Mode : uint8_t { Brightness, ColorTemp };
  Mode _mode = Mode::Brightness;

  // Rendering state
  bool _dirty = true;

  void drawFull(DisplayManager& display);
  void publishBrightness(PubSubClient& mqtt);
  void publishColorTemp(PubSubClient& mqtt);
  void publishToggle(PubSubClient& mqtt);
};
