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

#include "src/views/DashboardView.h"
#include "src/views/WaterHeaterView.h"
#include "src/views/FanView.h"
#include "src/views/LightView.h"
#include "src/views/SettingsView.h"

// --- Static view instances (no dynamic allocation) ---
static DashboardView   dashboardView;
static WaterHeaterView waterHeaterView;
static FanView         fanView;
static LightView       lightView;
static SettingsView    settingsView;

static DeviceView* views[] = { &dashboardView, &waterHeaterView, &fanView, &lightView, &settingsView };
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

// --- Touch hold tracking ---
// M5Unified's hold state machine requires perfectly still contact, which is
// impractical on the M5Dial's small screen — any slight finger movement
// pushes the state into flick/drag instead of hold. We use our own
// time-based detection: isPressed() stays true across touch/flick states,
// so we just check touch duration against a threshold.
static bool          touchHoldActive   = false;
static bool          touchWasPressed   = false;
static unsigned long touchStartMs      = 0;
static constexpr unsigned long HOLD_THRESHOLD_MS = 500;

// --- Input polling ---
// Reads hardware inputs and returns a single InputEvent.
// touchDetail is the shared touch state read once per loop (see loop()).
InputEvent pollInput(const m5::touch_detail_t& touch) {
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

  // Touch hold/release — suppress when BtnA is physically pressed (pressing
  // the button also contacts the capacitive screen).
  if (!M5Dial.BtnA.isPressed()) {
    bool isTouching = touch.isPressed();

    // Track touch start time for duration-based hold detection
    if (isTouching && !touchWasPressed) {
      touchStartMs = millis();
    }
    touchWasPressed = isTouching;

    // Hold: finger down for > threshold (works even with slight movement)
    if (!touchHoldActive && isTouching &&
        (millis() - touchStartMs) > HOLD_THRESHOLD_MS) {
      touchHoldActive = true;
      ev.type = InputType::TouchHoldStart;
      return ev;
    }
    // Release after hold
    if (touchHoldActive && !isTouching) {
      touchHoldActive = false;
      ev.type = InputType::TouchHoldEnd;
      return ev;
    }
    // Tap (only if not in a hold)
    if (!touchHoldActive && touch.wasClicked()) {
      ev.type = InputType::TouchTap;
      return ev;
    }
  }

  return ev;  // InputType::None
}

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* enableEncoder */);
  Serial.begin(115200);
  displayMgr.begin();

  // Show status BEFORE blocking — device is useless without network
  displayMgr.drawStatusOverlay("Connecting WiFi");
  IPAddress ip = connectivity.beginBlocking(ssid, password, mqtt_server, 1883,
                                             tz_offset_sec, dst_offset_sec);

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

  // 2. Read touch state ONCE — shared by Navigator (swipes) and pollInput (hold)
  auto touch = M5Dial.Touch.getDetail();

  // 3. Navigation — consume swipe gestures before passing input to active view
  navigator.processTouchInput(touch, displayMgr);

  // 4. Input — button, encoder, touch → active view
  InputEvent ev = pollInput(touch);
  if (ev.type != InputType::None) {
    navigator.activeView()->onInput(ev, connectivity.mqtt());
  }

  // 4. Active view update — dirty-check rendering + animation
  navigator.activeView()->update(now, displayMgr);

  // 5. Page indicator dots — drawn on top after view content
  displayMgr.drawPageDots(navigator.activeIndex(), navigator.viewCount());
}
