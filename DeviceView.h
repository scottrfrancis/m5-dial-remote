#pragma once
#include <PubSubClient.h>
#include "InputEvent.h"

class DisplayManager;  // forward declaration

class DeviceView {
public:
  virtual ~DeviceView() = default;

  // Identity
  virtual const char* name() const = 0;

  // MQTT — called on ALL views (not just active) so background views track state
  virtual void onMqttConnected(PubSubClient& mqtt) = 0;
  virtual bool onMqttMessage(const char* topic, const uint8_t* payload, unsigned int len) = 0;

  // Lifecycle — called when view becomes active/inactive via swipe navigation
  virtual void onActivate(DisplayManager& display) = 0;
  virtual void onDeactivate() = 0;

  // Input — called only on active view
  virtual void onInput(const InputEvent& event, PubSubClient& mqtt) = 0;

  // Rendering — called every loop iteration on active view only
  // Use internal dirty flags to minimize redraws. `now` is millis().
  virtual void update(unsigned long now, DisplayManager& display) = 0;
};
