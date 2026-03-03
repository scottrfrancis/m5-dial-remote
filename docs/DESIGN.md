# Refactor: Multi-Device Architecture for M5Stack Dial Remote

## Context

The current firmware is a single 300-line `.ino` file that was a quick-and-dirty implementation for one device (water heater). It works and is deployed. Now we're expanding to support N devices (fan, light, future devices) with swipe navigation, per-view input handling, and per-view MQTT routing. The monolith can't scale — display logic, network logic, input handling, and application state are all interleaved in `loop()` with 15+ globals.

This refactor decomposes the monolith into focused modules using a **coordinator + views** pattern, applying SOLID principles pragmatically for embedded.

---

## SOLID Principles Applied

| Principle | Application |
|---|---|
| **Single Responsibility** | Each class owns one concern: `ConnectivityManager` owns WiFi/MQTT, `Navigator` owns swipe/view switching, each `DeviceView` owns its device logic |
| **Open/Closed** | Adding a new device = implement `DeviceView` subclass + register it in the view array. Zero changes to framework code |
| **Liskov Substitution** | All views are interchangeable through the `DeviceView` interface — Navigator, MQTT router, and input dispatcher treat them identically |
| **Interface Segregation** | Views only see what they need: `PubSubClient&` for publishing, `DisplayManager&` for drawing. No god-object passed around |
| **Dependency Inversion** | The main loop depends on the `DeviceView` abstraction, not concrete views. Views depend on `DisplayManager` (abstraction over raw GFX) |

---

## File Layout

```
m5-dial-remote/
  m5-dial-remote.ino             # Coordinator — setup() + loop()
  environment.h                  # WiFi/MQTT credentials (gitignored)
  environment.h.example          # Credential template
  src/
    Config.h                     # Compile-time constants
    InputEvent.h                 # Input event enum + struct
    DeviceView.h                 # Abstract base class
    core/
      ConnectivityManager.h/.cpp # WiFi + MQTT lifecycle, exponential backoff
      DisplayManager.h/.cpp      # Display wrapper, overlays, page dots
      Navigator.h/.cpp           # Swipe detection, view switching
    gfx/
      Graphics.h/.cpp            # Drawing primitives (arcs, color wheel, arrows)
    views/
      WaterHeaterView.h/.cpp     # Water heater temp + pump control
      FanView.h/.cpp             # Ceiling fan speed + direction
      LightView.h/.cpp           # Light brightness + color temp (modal encoder)
      SettingsView.h/.cpp        # Display brightness (local-only, no MQTT)
  docs/
    DESIGN.md
    IDEA.md
```

The `.ino` must remain in the project root (Arduino requirement). Source files in `src/` are compiled recursively by arduino-cli. Cross-directory includes use relative paths (e.g. `../Config.h`, `../core/DisplayManager.h`).

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│  loop()  — 5-step pipeline                       │
│                                                   │
│  1. connectivity.tick()     → WiFi/MQTT health   │
│  2. navigator.processTouch() → consume swipes    │
│  3. pollInput()             → produce InputEvent │
│  4. activeView->onInput()   → view handles input │
│     activeView->update()    → view renders       │
│  5. displayMgr.drawPageDots() → navigation dots  │
└─────────────────────────────────────────────────┘
         │                    │
         ▼                    ▼
  ConnectivityManager    Navigator
  (WiFi + MQTT)         (swipe + view index)
         │
         ▼
  mqttCallback() ──► iterates ALL views ──► view.onMqttMessage()
```

---

## Key Interfaces

### DeviceView (abstract base — `DeviceView.h`)

```cpp
class DeviceView {
public:
  virtual ~DeviceView() = default;

  virtual const char* name() const = 0;

  // MQTT — called on ALL views (not just active)
  virtual void onMqttConnected(PubSubClient& mqtt) = 0;  // subscribe to topics
  virtual bool onMqttMessage(const char* topic,           // return true if handled
                             const uint8_t* payload, unsigned int len) = 0;

  // Lifecycle
  virtual void onActivate(DisplayManager& display) = 0;   // swiped-to; full redraw
  virtual void onDeactivate() = 0;                         // swiped-away

  // Input — called only on active view
  virtual void onInput(const InputEvent& event, PubSubClient& mqtt) = 0;

  // Rendering — called every loop iteration on active view
  virtual void update(unsigned long now, DisplayManager& display) = 0;
};
```

**Critical design decisions:**
- `onMqttMessage` called on ALL views so background views track state. When you swipe to a view, it already has current data.
- `onInput` and `update` only called on active view — inactive views consume zero CPU.
- Views receive `PubSubClient&` directly to publish. No intermediary — pragmatic, not over-abstracted.

### InputEvent (`InputEvent.h`)

```cpp
enum class InputType : uint8_t {
  None, ButtonPress, ButtonDoubleClick, ButtonHold,
  EncoderDelta, TouchTap, TouchHoldStart, TouchHoldEnd
};

struct InputEvent {
  InputType type = InputType::None;
  int16_t   delta = 0;    // encoder ticks
};
```

Swipe events are **not** in this enum — Navigator consumes them before input dispatch.

---

## Subsystem Details

### Config.h — Centralized Constants

Eliminates all magic numbers (120/120 center, radii, timing intervals, topic strings).

```cpp
namespace Cfg {
  constexpr int SCREEN_CX = 120, SCREEN_CY = 120;
  constexpr int ARC_RADIUS = 100, ARC_THICKNESS = 10;
  constexpr int WHEEL_R_INNER = 83, WHEEL_R_OUTER = 87;
  constexpr unsigned long ANIM_FRAME_MS = 50;     // 20 fps
  constexpr unsigned long WIFI_CHECK_MS = 5000;
  constexpr unsigned long MQTT_RETRY_MIN = 2000;
  constexpr unsigned long MQTT_RETRY_MAX = 30000;
  constexpr int SWIPE_THRESHOLD_PX = 40;
  constexpr int MAX_VIEWS = 8;
}
```

### ConnectivityManager — WiFi + MQTT Resilience

Extracts all connection logic from `loop()`. Owns `WiFiClient` + `PubSubClient`.

- `beginBlocking(ssid, pass, broker, port)` — called once in `setup()`, blocks until WiFi connects
- `tick(now)` — non-blocking; returns `true` if MQTT is connected and views should proceed
- `status()` / `statusMessage()` — for overlay display when disconnected
- `mqtt()` — returns `PubSubClient&` for views to publish through
- On MQTT reconnect: iterates all views calling `onMqttConnected()` so each re-subscribes

Preserves the existing exponential backoff (2s → 30s cap) and non-blocking WiFi check (every 5s).

### DisplayManager — Shared Display Resource

Thin wrapper. Views get direct `M5GFX&` access for custom rendering.

- `begin()` — set default font, datum, text color
- `clear()` — clear display
- `gfx()` — returns `M5GFX&` for direct drawing (arcs, custom graphics)
- `drawCenteredText(text, color, size)` — convenience
- `drawPageDots(current, total)` — iOS-style navigation dots at bottom
- `drawStatusOverlay(message)` — full-screen status (WiFi/MQTT disconnected)

### Graphics — Reusable Drawing Primitives (`Graphics.h/.cpp`)

Parameterized versions of `drawColorWheelFrame()` and `drawTempArc()`:

- `Gfx::drawValueArc(gfx, value, color, cx, cy, r, thickness)` — gauge arc, 0.0–1.0
- `Gfx::drawColorWheel(gfx, angleOffset, cx, cy, rInner, rOuter)` — spinning rainbow ring
- `Gfx::hsvToRgb565(hue, sat, val)` — color conversion
- `Gfx::drawArrow(gfx, cx, cy, up, color)` — directional arrow (fan view)
- `Gfx::drawPageDots(gfx, current, total, cy, spacing, radius)` — dot indicators

### Navigator — Swipe Detection + View Switching

- Reads `M5Dial.Touch.getDetail()` directly — swipes never reach views
- `processTouchInput(now)` — detects flick gestures, calls `switchTo()` on swipe
- `switchTo(index)` — calls `onDeactivate()` on old view, `onActivate()` on new view
- `activeView()` / `activeIndex()` / `viewCount()` — accessors

### MQTT Routing

PubSubClient requires a C-style callback. The routing is a free function:

```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  for (uint8_t i = 0; i < viewCount; i++) {
    if (views[i]->onMqttMessage(topic, payload, length)) break;
  }
}
```

Each view does `strcmp()` on its own topics and returns `true` if handled. Linear scan is fine for <10 views.

---

## Concrete Views

### WaterHeaterView — Port of Existing Code

| Aspect | Detail |
|---|---|
| Sub topics | `water/temp`, `water/recirc` |
| Pub topic | `water/recirc/cmd` (**changed** — separate from sub to avoid echo) |
| State | `_tempF`, `_pumpRunning`, `_wheelAngle`, `_lastFrameTime` |
| Button | Publish `{"start": 5}` to `water/recirc/cmd` |
| Encoder | Unused |
| Display | Temp text (center), value arc (90–140°F with color bands), color wheel when pump on |
| Animation | Color wheel at 20fps, draws over itself without clearing |
| Dirty flag | `_dirty` set by `onMqttMessage` and `onActivate`, cleared after `drawFull()` |

**Bug fix included:** Separate pub/sub topics. Currently `pump_topic` is used for both, causing the device to receive its own publishes and potentially flipping `pumpRunning` to false. Requires a corresponding HA automation change (`water/recirc/cmd` trigger).

### FanView — New

| Aspect | Detail |
|---|---|
| Sub topics | `fan/bedroom/state` |
| Pub topic | `fan/bedroom/command` |
| State | `_speed` (0–6), `_direction` ("forward"/"reverse") |
| Button | Toggle fan on/off |
| Encoder | Cycle speed 0–6 |
| ButtonHold | Toggle direction |
| Display | Speed arc (0–6 mapped to gauge), direction arrow (up/down) |

### LightView — New (Modal Encoder)

| Aspect | Detail |
|---|---|
| Sub topics | `fan/bedroom/light` |
| Pub topic | `fan/bedroom/command` |
| State | `_on`, `_brightness` (0–255), `_colorTemp` (153–500 mireds), `_mode` enum |
| Button | Toggle on/off |
| Encoder (Brightness mode) | Adjust brightness |
| Encoder (ColorTemp mode) | Adjust color temperature |
| TouchHoldStart | Enter color temp mode |
| TouchHoldEnd | Exit color temp mode |
| Display | Brightness arc (normal mode), warm-to-cool gradient arc (color temp mode) |

### SettingsView — Local Device Settings

| Aspect | Detail |
|---|---|
| Sub topics | None |
| Pub topic | None |
| State | `_brightness` (0–255) |
| Encoder | Adjust display brightness |
| Button | Unused |
| Display | "Settings" header, brightness percentage, value arc (WHITE) |
| MQTT | No-op — all settings are local to the device |

---

## Main .ino After Refactor (~70 lines)

```cpp
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

static WaterHeaterView waterHeaterView;
static FanView         fanView;
static LightView       lightView;
static SettingsView    settingsView;

static DeviceView* views[] = { &waterHeaterView, &fanView, &lightView, &settingsView };  // 4 views
static constexpr uint8_t VIEW_COUNT = sizeof(views) / sizeof(views[0]);

static ConnectivityManager connectivity;
static DisplayManager      displayMgr;
static Navigator           navigator;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  for (uint8_t i = 0; i < VIEW_COUNT; i++) {
    if (views[i]->onMqttMessage(topic, payload, length)) break;
  }
}

InputEvent pollInput() { /* read BtnA, Encoder, Touch → return InputEvent */ }

void setup() {
  auto cfg = M5.config();
  M5Dial.begin(cfg, true /* enableEncoder */);
  Serial.begin(115200);
  displayMgr.begin();

  IPAddress ip = connectivity.beginBlocking(ssid, password, mqtt_server, 1883);
  // show IP briefly...

  connectivity.mqtt().setCallback(mqttCallback);
  connectivity.setViews(views, VIEW_COUNT);
  navigator.setViews(views, VIEW_COUNT);
  navigator.activeView()->onActivate(displayMgr);
}

void loop() {
  unsigned long now = millis();
  M5Dial.update();

  // 1. Connectivity
  if (!connectivity.tick(now)) {
    displayMgr.drawStatusOverlay(connectivity.statusMessage());
    return;
  }
  connectivity.mqtt().loop();

  // 2. Navigation (consumes swipes)
  navigator.processTouchInput(now);

  // 3. Input → active view
  InputEvent ev = pollInput();
  if (ev.type != InputType::None)
    navigator.activeView()->onInput(ev, connectivity.mqtt());

  // 4. Active view render
  navigator.activeView()->update(now, displayMgr);

  // 5. Page dots overlay
  displayMgr.drawPageDots(navigator.activeIndex(), navigator.viewCount());
}
```

---

## Design Constraints

- **No heap allocation in hot path** — all views statically allocated, `StaticJsonDocument` on stack
- **No STL containers** — fixed C arrays for view list
- **One vtable lookup per loop** — negligible at 240 MHz
- **Arduino IDE compatible** — all files in project root, `.ino` is the entry point
- **C++17** — `constexpr`, `enum class`, `auto`, but no exceptions or RTTI

---

## Bug Fixes and Gotchas

### M5Dial.begin() — Encoder Must Be Explicitly Enabled

`M5Dial.begin(cfg)` defaults `enableEncoder` to `false`. The encoder GPIO pin register pointers are left null, and any call to `Encoder.readAndReset()` dereferences `pin1_register` at `0x00000000`, causing a `LoadProhibited` crash.

**Fix:** Always pass `true` for the encoder parameter:

```cpp
M5Dial.begin(cfg, true /* enableEncoder */);
```

The crash manifests as `Guru Meditation Error: Core 1 panic'ed (LoadProhibited)` with `EXCVADDR: 0x00000000`. The backtrace points to `ENCODER::update()` in `Encoder.h:255` where `DIRECT_PIN_READ(arg->pin1_register, arg->pin1_bitmask)` reads through a null pointer.

This was not caught in the original monolith because the old code may not have used the encoder, or used a different `begin()` overload.

### Shared Pub/Sub Topic — MQTT Echo

The original code published `{"start": 5}` to `water/recirc` **and** subscribed to `water/recirc` for `{"pump": "on/off"}`. The device receives its own publish, `callback()` tries to read `doc["pump"]` from `{"start": 5}`, finds it absent, and sets `pumpRunning = false`.

**Fix:** Publish to `water/recirc/cmd` (new topic). Requires updating the HA automation to trigger on `water/recirc/cmd` instead of `water/recirc`.

### ESP32-S3 mDNS Unreliability

mDNS hostname resolution (e.g. `vault.local`) is unreliable on ESP32-S3. The MQTT broker address should be a raw IP in `environment.h`. If the broker IP changes, update `environment.h` and reflash.

### M5GFX API — No getTextSize()

`M5GFX` does not expose `getTextSize()`. Don't try to save/restore text size around draw calls — just set the size you need directly.

---

## Implementation Sequence

| Step | What | Verifiable At |
|---|---|---|
| 1 | Create `Config.h`, `InputEvent.h` (header-only) | Compiles |
| 2 | Create `Graphics.h/.cpp` — extract `drawColorWheelFrame`, `drawTempArc`, parameterize | Compiles, visual test on device |
| 3 | Create `DisplayManager.h/.cpp` — thin wrapper + page dots + status overlay | Compiles |
| 4 | Create `DeviceView.h` — abstract base class | Compiles |
| 5 | Create `ConnectivityManager.h/.cpp` — extract WiFi/MQTT from setup()/loop() | WiFi connects, MQTT connects, backoff works |
| 6 | Create `WaterHeaterView.h/.cpp` — port existing logic into view interface | **Identical behavior to current monolith** |
| 7 | Rewrite `.ino` — 5-step loop, wire subsystems | Full regression: temp display, arc, pump animation, button, reconnect |
| 8 | Create `Navigator.h/.cpp` — swipe detection, view switching, page dots | Swipe between views (water heater only initially) |
| 9 | Create `FanView.h/.cpp` | Fan control works via MQTT |
| 10 | Create `LightView.h/.cpp` with modal encoder | Light control + color temp mode works |
| 11 | Update HA automations | Separate pub topic for water/recirc, new automations for fan/light |

**Steps 1–7 produce identical behavior to the current monolith.** This is a safe incremental migration — verify at step 7 before adding new features.

---

## Verification Plan

1. **After step 7:** Flash device, verify water heater temp display, arc colors, pump animation, button publish, WiFi/MQTT reconnect with backoff — all identical to current behavior
2. **After step 8:** Verify swipe left/right navigates (single view wraps or no-ops), page dots render
3. **After steps 9–10:** Publish test MQTT messages to fan/light topics, verify display updates, encoder/button/hold interactions
4. **Throughout:** Serial monitor for MQTT connect/disconnect/backoff messages, verify no heap fragmentation with `ESP.getFreeHeap()`

---

## Navigation and UX

View order (swipe left to right): Water Heater → Fan → Light → Settings

---

## Files Modified

| File | Notes |
|---|---|
| `WaterHeaterView.h` / `.cpp` | Port of existing logic |
| `FanView.h` / `.cpp` | New — fan speed and direction control |
| `LightView.h` / `.cpp` | New — brightness and color temp (modal encoder) |
| `SettingsView.h` / `.cpp` | New — settings view (brightness control) |
