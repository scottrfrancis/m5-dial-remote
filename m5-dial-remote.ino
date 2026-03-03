/*
  M5Stack Dial — Multi-Device IoT Remote

  Coordinator: wires connectivity, navigation, input, and views.
  See docs/DESIGN.md for architecture.
*/

#include <M5Dial.h>
#include "environment.h"
#include "src/Config.h"
#include "src/InputEvent.h"
#include "src/core/ConnectivityManager.h"
#include "src/core/DisplayManager.h"
#include "src/core/Navigator.h"

#include "src/views/WaterHeaterView.h"
#include "src/views/FanView.h"
#include "src/views/LightView.h"
#include "src/views/SettingsView.h"

// --- Static view instances (no dynamic allocation) ---
static WaterHeaterView waterHeaterView;
static FanView         fanView;
static LightView       lightView;
static SettingsView    settingsView;

static DeviceView* views[] = { &waterHeaterView, &fanView, &lightView, &settingsView };
static constexpr uint8_t VIEW_COUNT = sizeof(views) / sizeof(views[0]);

// --- Subsystems ---
static ConnectivityManager connectivity;
static DisplayManager      displayMgr;
static Navigator           navigator;

// --- MQTT callback (free function required by PubSubClient) ---
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  for (uint8_t i = 0; i < VIEW_COUNT; i++) {
    if (views[i]->onMqttMessage(topic, reinterpret_cast<const uint8_t*>(payload), length)) {
      break;
    }
  }
}

// --- Input polling ---
// Reads hardware inputs and returns a single InputEvent.
// Called after Navigator has consumed swipe gestures.
InputEvent pollInput() {
  InputEvent ev;

  if (M5Dial.BtnA.wasPressed())       { ev.type = InputType::ButtonPress;       return ev; }
  if (M5Dial.BtnA.wasDoubleClicked()) { ev.type = InputType::ButtonDoubleClick; return ev; }
  if (M5Dial.BtnA.wasHold())          { ev.type = InputType::ButtonHold;        return ev; }

  // Encoder: readAndReset() returns accumulated ticks since last call
  int16_t enc = M5Dial.Encoder.readAndReset();
  if (enc != 0) {
    ev.type  = InputType::EncoderDelta;
    ev.delta = enc;
    return ev;
  }

  // Touch tap/hold (swipes already consumed by Navigator)
  auto touch = M5Dial.Touch.getDetail();
  if (touch.wasClicked()) { ev.type = InputType::TouchTap;       return ev; }
  if (touch.wasHold())    { ev.type = InputType::TouchHoldStart; return ev; }

  return ev;  // InputType::None
}

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* enableEncoder */);
  Serial.begin(115200);
  displayMgr.begin();

  // Show status BEFORE blocking — device is useless without network
  displayMgr.drawStatusOverlay("Connecting WiFi");
  IPAddress ip = connectivity.beginBlocking(ssid, password, mqtt_server, 1883);

  // Show IP briefly so the user can confirm network identity
  displayMgr.clear();
  char ipStr[20];
  ip.toString().toCharArray(ipStr, sizeof(ipStr));
  displayMgr.drawCenteredText(ipStr, GREEN, 1.0f);
  delay(500);

  // Wire MQTT callback and register views for subscription on (re)connect
  connectivity.mqtt().setCallback(mqttCallback);
  connectivity.setViews(views, VIEW_COUNT);

  // Wire navigator and activate first view
  navigator.setViews(views, VIEW_COUNT);
  navigator.activeView()->onActivate(displayMgr);
}

void loop() {
  unsigned long now = millis();
  M5Dial.update();

  // 1. Connectivity — WiFi/MQTT resilience; tick() calls _mqtt.loop() internally
  if (!connectivity.tick(now)) {
    displayMgr.drawStatusOverlay(connectivity.statusMessage());
    return;
  }

  // 2. Navigation — consume swipe gestures before passing input to active view
  navigator.processTouchInput(displayMgr);

  // 3. Input — button, encoder, touch → active view
  InputEvent ev = pollInput();
  if (ev.type != InputType::None) {
    navigator.activeView()->onInput(ev, connectivity.mqtt());
  }

  // 4. Active view update — dirty-check rendering + animation
  navigator.activeView()->update(now, displayMgr);

  // 5. Page indicator dots — drawn on top after view content
  displayMgr.drawPageDots(navigator.activeIndex(), navigator.viewCount());
}
