/*
Water Heater temp monitor and remote recirc control

  WiFi API: https://docs.m5stack.com/en/arduino/m5core/wifi
  Display API: https://docs.m5stack.com/en/arduino/m5gfx/m5gfx_functions

*/

#include <ArduinoJson.h>  // at the top of your sketch
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

void startColorWheel() {
  // pumpRunning = true;
  colorWheelAngle = 0;
  lastFrameTime = millis();
}

void drawColorWheelFrame(int angleOffset) {
  int cx = 120;
  int cy = 120;
  int r1 = 83;
  int r2 = 87;

  // No clearing — draw over existing arc

  for (int i = 0; i < 360; i += 10) {
    int angleStart = (i + angleOffset) % 360;
    int angleEnd   = (i + 10 + angleOffset) % 360;

    // Fixed hue for each segment
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

    // Handle wrap-around
    if (angleEnd < angleStart) {
      M5Dial.Display.fillArc(cx, cy, r1, r2, angleStart, 360, color);
      M5Dial.Display.fillArc(cx, cy, r1, r2, 0, angleEnd, color);
    } else {
      M5Dial.Display.fillArc(cx, cy, r1, r2, angleStart, angleEnd, color);
    }
  }
}

void drawTempArc(float tempF) {
  // Clamp and map temp (e.g., 90°F–140°F maps to 0–270°)
  float clamped = constrain(tempF, 90.0, 140.0);
  int angle = map(clamped, 90, 140, 0, 270);

  // Define arc appearance
  int centerX = 120;
  int centerY = 120;
  int radius = 100;
  int thickness = 10;

  // Choose color based on temp
  uint16_t arcColor = (tempF < 100) ? BLUE : (tempF < 120) ? YELLOW : RED;

  // Draw arc using M5GFX
  M5Dial.Display.fillArc(centerX, centerY, radius - thickness, radius, 135, 135 + angle, arcColor);
}

float currentTempF = 50.0;
// Callback when a message is received
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';  // Null-terminate the payload
  Serial.printf("Message arrived [%s]: %s\n", topic, payload);

  // Parse JSON
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  // extract payload and update internal state vars/gobals
  if (strcmp(topic, temp_topic) == 0) {
    currentTempF = doc["temp_degF"];
  } else if (strcmp(topic, pump_topic) == 0) {
    const char* pumpState = doc["pump"];
    if (pumpState && strcmp(pumpState, "on") == 0) {
      if (pumpRunning == false) {
        startColorWheel();  // Start the animation
      }
      pumpRunning = true;
    }
    else {
      pumpRunning = false;
    }
  }
}

// Connect to MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "M5DialClient-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(temp_topic);
      client.subscribe(pump_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg);

  M5Dial.Display.setTextColor(GREEN);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.setTextSize(1);

  Serial.begin(115200);

  const char *conn_msg = "Connecting WiFi";
  Serial.print(conn_msg);
  M5Dial.Display.drawString(conn_msg, M5Dial.Display.width() / 2,
                            M5Dial.Display.height() / 2);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
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
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  M5Dial.update();

  // Redraw 
  bool shouldRedraw = false;

  // Check if temperature or pump state changed
  if (abs(currentTempF - lastTempF) >= 0.1 || pumpRunning != lastPumpRunning) {
    shouldRedraw = true;
    lastTempF = currentTempF;
    lastPumpRunning = pumpRunning;
  }

  // Also redraw if animation needs a new frame
  unsigned long now = millis();
  if (pumpRunning && (now - lastFrameTime > 50)) {
    colorWheelAngle = (colorWheelAngle + 8) % 360;
    lastFrameTime = now;
    // shouldRedraw = true;
  }

  if (shouldRedraw) {
    // Clear screen once per update cycle
    M5Dial.Display.clearDisplay();
  }
    // Draw temp text
    M5Dial.Display.setTextColor(GREEN);
    M5Dial.Display.setTextSize(2);
    int tempWhole = static_cast<int>(round(currentTempF));
    M5Dial.Display.drawString(String(tempWhole) + " F", 120, 120);

    // Draw temp arc
    drawTempArc(currentTempF);

    // Draw pump animation ring, if active
    if (pumpRunning) {
      drawColorWheelFrame(colorWheelAngle);
    }
  // }

  if (M5Dial.BtnA.wasPressed()) {
    Serial.println("Button pressed — sending start command");

    // Build and publish payload
    StaticJsonDocument<64> doc;
    doc["start"] = 5;

    char payload[64];
    serializeJson(doc, payload);

    if (client.publish(pump_topic, payload)) {
      Serial.println("MQTT publish successful");

      M5Dial.Display.clearDisplay();
      M5Dial.Display.drawString("Recirc", 120, 120);
      // delay(500);  // Brief pause to show feedback
    } else {
      Serial.println("MQTT publish failed");
    }
  }
}
