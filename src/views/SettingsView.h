#pragma once

#include "../DeviceView.h"
#include "../core/DisplayManager.h"
#include "../InputEvent.h"

// ---------------------------------------------------------------------------
// SettingsView — local-only settings page (no MQTT interaction).
//
// Currently exposes a single setting: display brightness (0-255).
// The encoder adjusts brightness in real time; no button action is defined.
// ---------------------------------------------------------------------------

class SettingsView : public DeviceView {
public:
  const char* name() const override { return "Settings"; }

  void onMqttConnected(PubSubClient& mqtt) override;
  bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) override;
  void onActivate(DisplayManager& display) override;
  void onDeactivate() override;
  void onInput(const InputEvent& event, PubSubClient& mqtt) override;
  void update(unsigned long now, DisplayManager& display) override;

private:
  uint8_t _brightness = 128;  // 0-255, default mid-level
  bool    _dirty      = true;

  void drawFull(DisplayManager& display);
  void applyBrightness();
};
