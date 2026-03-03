#include "SettingsView.h"

#include <Arduino.h>
#include <M5Dial.h>

#include "../Config.h"
#include "../core/DisplayManager.h"
#include "../gfx/Graphics.h"

// ---------------------------------------------------------------------------
// MQTT — no topics owned by this view
// ---------------------------------------------------------------------------

void SettingsView::onMqttConnected(PubSubClient& mqtt) {
  // no-op: SettingsView has no MQTT topics
}

bool SettingsView::onMqttMessage(const char* topic,
                                 const uint8_t* payload,
                                 unsigned int len) {
  return false;  // no topics owned
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void SettingsView::onActivate(DisplayManager& display) {
  _dirty = true;  // force full redraw when swiped to
}

void SettingsView::onDeactivate() {
  // nothing needed
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void SettingsView::onInput(const InputEvent& event, PubSubClient& mqtt) {
  switch (event.type) {
    case InputType::EncoderDelta: {
      int adjusted  = static_cast<int>(_brightness) + event.delta * 4;
      _brightness   = static_cast<uint8_t>(constrain(adjusted, 0, 255));
      applyBrightness();
      _dirty = true;
      break;
    }

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Update (called every loop on the active view)
// ---------------------------------------------------------------------------

void SettingsView::update(unsigned long now, DisplayManager& display) {
  if (_dirty) {
    drawFull(display);
    _dirty = false;
  }
}

// ---------------------------------------------------------------------------
// Private — apply brightness to hardware
// ---------------------------------------------------------------------------

void SettingsView::applyBrightness() {
  M5Dial.Display.setBrightness(_brightness);
}

// ---------------------------------------------------------------------------
// Private — full repaint
// ---------------------------------------------------------------------------

void SettingsView::drawFull(DisplayManager& display) {
  display.clear();

  // "Settings" label near the top
  display.gfx().setTextDatum(middle_center);
  display.gfx().setTextSize(0.7f);
  display.gfx().setTextColor(0x7BEF);
  display.gfx().drawString("Settings", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY - 50);

  // Brightness percentage — large centered value
  char buf[8];
  int pct = static_cast<int>(round(_brightness / 255.0f * 100.0f));
  snprintf(buf, sizeof(buf), "%d%%", pct);
  display.drawCenteredText(buf, WHITE, 2.0f);

  // Brightness arc
  float normalized = _brightness / 255.0f;
  Gfx::drawValueArc(display.gfx(),
                    normalized,
                    WHITE,
                    Cfg::DISPLAY_CX,
                    Cfg::DISPLAY_CY,
                    Cfg::ARC_RADIUS,
                    Cfg::ARC_THICKNESS,
                    Cfg::ARC_START_DEG,
                    Cfg::ARC_SPAN_DEG);

  // "BRIGHTNESS" label below center
  display.gfx().setTextDatum(middle_center);
  display.gfx().setTextSize(0.6f);
  display.gfx().setTextColor(0x7BEF);
  display.gfx().drawString("BRIGHTNESS", Cfg::DISPLAY_CX, Cfg::DISPLAY_CY + 40);
}
