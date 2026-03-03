/*
Water Heater temp monitor and remote recirc control

  WiFi API: https://docs.m5stack.com/en/arduino/m5core/wifi
  Display API: https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_functions

*/

#include <ArduinoJson.h>
#include <M5Dial.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "environment.h"


// MQTT broker info
const char *temp_topic  = "water/temp";
const char *pump_topic  = "water/recirc";

WiFiClient espClient;
PubSubClient client(espClient);

bool pumpRunning = false;
unsigned long lastFrameTime = 0;
int colorWheelAngle = 0;

// Connection resilience state
unsigned long lastWifiCheck = 0;
unsigned long lastMqttAttempt = 0;
unsigned long mqttRetryInterval = 2000;
const unsigned long MQTT_MAX_RETRY = 30000;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
bool wifiConnected = false;
bool mqttConnected = false;

void startColorWheel() {
  colorWheelAngle = 0;
  lastFrameTime = millis();
}

void drawColorWheelFrame(int angleOffset) {
  int cx = 120;
  int cy = 120;
  int r1 = 83;
  int r2 = 87;

  for (int i = 0; i < 360; i += 10) {
    int angleStart = (i + angleOffset) % 360;
    int angleEnd   = (i + 10 + angleOffset) % 360;

    float hue = i / 360.0f;
    uint8_t r, g, b;

    int h = int(hue * 6);
    float f = hue * 6 - h;
    uint8_t v = 255;
    uint8_t p = 0;
    uint8_t q = 255 * (1 - f);
    uint8_t t = 255 * f;

    switch (h % 6) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
    }

    uint16_t color = M5Dial.Display.color565(r, g, b);

    if (angleEnd < angleStart) {
      M5Dial.Display.fillArc(cx, cy, r1, r2, angleStart, 360, color);
      M5Dial.Display.fillArc(cx, cy, r1, r2, 0, angleEnd, color);
    } else {
      M5Dial.Display.fillArc(cx, cy, r1, r2, angleStart, angleEnd, color);
    }
  }
}

void drawTempArc(float tempF) {
  float clamped = constrain(tempF, 90.0, 140.0);
  int angle = map(clamped, 90, 140, 0, 270);

  int centerX = 120;
  int centerY = 120;
  int radius = 100;
  int thickness = 10;

  uint16_t arcColor = (tempF < 100) ? BLUE : (tempF < 120) ? YELLOW : RED;

  M5Dial.Display.fillArc(centerX, centerY, radius - thickness, radius, 135, 135 + angle, arcColor);
}

void drawStatusMessage(const char *msg) {
  M5Dial.Display.clearDisplay();
  M5Dial.Display.setTextColor(YELLOW);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString(msg, 120, 120);
}

float currentTempF = 50.0;

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  Serial.printf("Message arrived [%s]: %s\n", topic, payload);

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (strcmp(topic, temp_topic) == 0) {
    currentTempF = doc["temp_degF"];
  } else if (strcmp(topic, pump_topic) == 0) {
    const char* pumpState = doc["pump"];
    if (pumpState && strcmp(pumpState, "on") == 0) {
      if (pumpRunning == false) {
        startColorWheel();
      }
      pumpRunning = true;
    }
    else {
      pumpRunning = false;
    }
  }
}

// Non-blocking MQTT reconnect — single attempt per call
bool mqttReconnect() {
  Serial.print("Attempting MQTT connection...");
  String clientId = "M5DialClient-";
  clientId += String(random(0xffff), HEX);

  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    client.subscribe(temp_topic);
    client.subscribe(pump_topic);
    mqttRetryInterval = 2000;  // Reset backoff on success
    return true;
  }

  Serial.printf("failed, rc=%d\n", client.state());
  return false;
}

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg);

  M5Dial.Display.setTextColor(GREEN);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.setTextSize(1);

  Serial.begin(115200);

  // Blocking WiFi connect on first boot — device needs network to function
  drawStatusMessage("Connecting WiFi");
  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  wifiConnected = true;
  Serial.println("Connected!");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  delay(500);
  M5Dial.Display.clearDisplay();
  M5Dial.Display.drawString(ip.toString(), M5Dial.Display.width() / 2,
                          M5Dial.Display.height() / 2);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

float lastTempF = -999;
bool lastPumpRunning = false;

void loop() {
  unsigned long now = millis();
  M5Dial.update();

  // --- WiFi resilience: check periodically, reconnect non-blocking ---
  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    bool currentWifiStatus = (WiFi.status() == WL_CONNECTED);

    if (!currentWifiStatus && wifiConnected) {
      // WiFi just dropped
      wifiConnected = false;
      Serial.println("WiFi disconnected — attempting reconnect");
      drawStatusMessage("WiFi lost...");
      WiFi.reconnect();
    } else if (!currentWifiStatus && !wifiConnected) {
      // Still disconnected — retry
      Serial.println("WiFi still disconnected — retrying");
      WiFi.reconnect();
    } else if (currentWifiStatus && !wifiConnected) {
      // WiFi just recovered
      wifiConnected = true;
      Serial.println("WiFi reconnected");
      Serial.println(WiFi.localIP());
    }
  }

  // --- MQTT resilience: non-blocking reconnect with exponential backoff ---
  if (wifiConnected && !client.connected()) {
    if (mqttConnected) {
      // MQTT just dropped
      mqttConnected = false;
      Serial.println("MQTT disconnected");
    }
    if (now - lastMqttAttempt >= mqttRetryInterval) {
      lastMqttAttempt = now;
      if (mqttReconnect()) {
        mqttConnected = true;
      } else {
        // Exponential backoff, capped
        mqttRetryInterval = min(mqttRetryInterval * 2, MQTT_MAX_RETRY);
        Serial.printf("MQTT retry in %lu ms\n", mqttRetryInterval);
      }
    }
    // Show status while disconnected, then return to skip normal drawing
    if (!mqttConnected) {
      drawStatusMessage("MQTT...");
      return;
    }
  } else if (wifiConnected && client.connected() && !mqttConnected) {
    mqttConnected = true;
  }

  if (!wifiConnected) {
    return;  // Skip everything until WiFi is back
  }

  client.loop();

  // --- Redraw logic ---
  bool stateChanged = false;

  if (abs(currentTempF - lastTempF) >= 0.1 || pumpRunning != lastPumpRunning) {
    stateChanged = true;
    lastTempF = currentTempF;
    lastPumpRunning = pumpRunning;
  }

  // Full redraw on state change (clear + repaint everything)
  if (stateChanged) {
    M5Dial.Display.clearDisplay();

    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextSize(2);
    int tempWhole = static_cast<int>(round(currentTempF));
    M5Dial.Display.drawString(String(tempWhole) + " F", 120, 120);

    drawTempArc(currentTempF);

    if (pumpRunning) {
      drawColorWheelFrame(colorWheelAngle);
    }
  }

  // Animation frame update — color wheel draws over itself without clearing,
  // so no clearDisplay() needed here (avoids full-screen flicker at 20fps)
  if (pumpRunning && (now - lastFrameTime > 50)) {
    colorWheelAngle = (colorWheelAngle + 8) % 360;
    lastFrameTime = now;
    drawColorWheelFrame(colorWheelAngle);
  }

  if (M5Dial.BtnA.wasPressed()) {
    Serial.println("Button pressed — sending start command");

    StaticJsonDocument<64> doc;
    doc["start"] = 5;

    char payload[64];
    serializeJson(doc, payload);

    if (client.publish(pump_topic, payload)) {
      Serial.println("MQTT publish successful");

      M5Dial.Display.clearDisplay();
      M5Dial.Display.drawString("Recirc", 120, 120);
    } else {
      Serial.println("MQTT publish failed");
    }
  }
}
