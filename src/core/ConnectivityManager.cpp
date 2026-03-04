#include "ConnectivityManager.h"
#include "../DeviceView.h"

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

IPAddress ConnectivityManager::beginBlocking(const char* ssid,
                                              const char* password,
                                              const char* broker,
                                              uint16_t    port,
                                              long        gmtOffset_sec,
                                              int         dstOffset_sec) {
  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  _wifiConnected = true;
  Serial.println("Connected!");

  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  // Configure NTP — getLocalTime() will sync asynchronously
  configTime(gmtOffset_sec, dstOffset_sec, "pool.ntp.org");
  Serial.println("NTP configured");

  _mqtt.setServer(broker, port);

  return ip;
}

bool ConnectivityManager::tick(unsigned long now) {
  // --- WiFi resilience: check periodically, reconnect non-blocking ---
  if (now - _lastWifiCheck >= Cfg::WIFI_CHECK_INTERVAL_MS) {
    _lastWifiCheck = now;
    bool currentWifiStatus = (WiFi.status() == WL_CONNECTED);

    if (!currentWifiStatus && _wifiConnected) {
      // WiFi just dropped
      _wifiConnected = false;
      Serial.println("WiFi disconnected — attempting reconnect");
      WiFi.reconnect();
    } else if (!currentWifiStatus && !_wifiConnected) {
      // Still disconnected — retry
      Serial.println("WiFi still disconnected — retrying");
      WiFi.reconnect();
    } else if (currentWifiStatus && !_wifiConnected) {
      // WiFi just recovered
      _wifiConnected = true;
      Serial.println("WiFi reconnected");
      Serial.println(WiFi.localIP());
    }
  }

  // --- MQTT resilience: non-blocking reconnect with exponential backoff ---
  if (_wifiConnected && !_mqtt.connected()) {
    if (_mqttConnected) {
      // MQTT just dropped
      _mqttConnected = false;
      Serial.println("MQTT disconnected");
    }
    if (now - _lastMqttAttempt >= _mqttRetryInterval) {
      _lastMqttAttempt = now;
      if (mqttReconnect()) {
        _mqttConnected = true;
      } else {
        // Exponential backoff, capped
        _mqttRetryInterval = min(_mqttRetryInterval * 2, Cfg::MQTT_RETRY_MAX_MS);
        Serial.printf("MQTT retry in %lu ms\n", _mqttRetryInterval);
      }
    }
    // Show status while disconnected
    if (!_mqttConnected) {
      _status = Status::MqttRetrying;
      return false;
    }
  } else if (_wifiConnected && _mqtt.connected() && !_mqttConnected) {
    _mqttConnected = true;
  }

  if (!_wifiConnected) {
    _status = Status::WifiLost;
    return false;
  }

  _mqtt.loop();
  _status = Status::OK;
  return true;
}

ConnectivityManager::Status ConnectivityManager::status() const {
  return _status;
}

const char* ConnectivityManager::statusMessage() const {
  switch (_status) {
    case Status::WifiLost:     return "WiFi lost...";
    case Status::MqttRetrying: return "MQTT...";
    default:                   return nullptr;
  }
}

PubSubClient& ConnectivityManager::mqtt() {
  return _mqtt;
}

void ConnectivityManager::setViews(DeviceView** views, uint8_t count) {
  _views     = views;
  _viewCount = count;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool ConnectivityManager::mqttReconnect() {
  Serial.print("Attempting MQTT connection...");
  String clientId = "M5DialClient-";
  clientId += String(random(0xffff), HEX);

  if (_mqtt.connect(clientId.c_str())) {
    Serial.println("connected");
    _mqttRetryInterval = Cfg::MQTT_RETRY_MIN_MS;  // Reset backoff on success
    subscribeAllViews();
    return true;
  }

  Serial.printf("failed, rc=%d\n", _mqtt.state());
  return false;
}

void ConnectivityManager::subscribeAllViews() {
  for (uint8_t i = 0; i < _viewCount; ++i) {
    _views[i]->onMqttConnected(_mqtt);
  }
}
