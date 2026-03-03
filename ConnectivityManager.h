#pragma once
#include <WiFi.h>
#include <PubSubClient.h>
#include "Config.h"

class DeviceView;

class ConnectivityManager {
public:
  // Blocking initial WiFi connect + MQTT server config.
  // Called once in setup(). Blocks until WiFi connects (matching original behavior).
  // Does NOT attempt MQTT connection — that's deferred to tick().
  // Returns the assigned IP address so the caller can display it.
  IPAddress beginBlocking(const char* ssid, const char* password,
                          const char* broker, uint16_t port);

  // Non-blocking tick. Call every loop() iteration.
  // Returns true if MQTT is connected and normal operation should proceed.
  // Returns false if WiFi or MQTT is down (caller should show status overlay and skip).
  bool tick(unsigned long now);

  // Status for display overlay
  enum class Status : uint8_t { OK, WifiLost, MqttRetrying };
  Status status() const;
  const char* statusMessage() const;

  // Access the MQTT client (for views to publish, and for main .ino to set callback)
  PubSubClient& mqtt();

  // Register views so ConnectivityManager can call onMqttConnected() on reconnect
  void setViews(DeviceView** views, uint8_t count);

private:
  WiFiClient   _espClient;
  PubSubClient _mqtt{_espClient};

  DeviceView** _views     = nullptr;
  uint8_t      _viewCount = 0;

  bool          _wifiConnected    = false;
  bool          _mqttConnected    = false;
  unsigned long _lastWifiCheck    = 0;
  unsigned long _lastMqttAttempt  = 0;
  unsigned long _mqttRetryInterval = Cfg::MQTT_RETRY_MIN_MS;

  Status _status = Status::OK;

  bool mqttReconnect();
  void subscribeAllViews();
};
