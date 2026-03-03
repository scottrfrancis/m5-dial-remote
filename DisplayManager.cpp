#include "DisplayManager.h"
#include "Config.h"
#include "Graphics.h"

void DisplayManager::begin() {
  M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(GREEN);
  M5Dial.Display.setTextSize(1);
}

void DisplayManager::clear() {
  M5Dial.Display.clearDisplay();
}

M5GFX& DisplayManager::gfx() {
  return M5Dial.Display;
}

void DisplayManager::drawCenteredText(const char* text, uint16_t color, float textSize) {
  M5Dial.Display.setTextColor(color);
  M5Dial.Display.setTextSize(textSize);
  M5Dial.Display.drawString(text, Cfg::DISPLAY_CX, Cfg::DISPLAY_CY);
}

void DisplayManager::drawPageDots(uint8_t current, uint8_t total) {
  Gfx::drawPageDots(M5Dial.Display,
                    current,
                    total,
                    Cfg::DOT_Y_OFFSET,
                    Cfg::DOT_RADIUS_PX,
                    Cfg::DOT_SPACING_PX);
}

void DisplayManager::drawStatusOverlay(const char* message) {
  M5Dial.Display.clearDisplay();
  M5Dial.Display.setTextColor(YELLOW);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString(message, Cfg::DISPLAY_CX, Cfg::DISPLAY_CY);
}
