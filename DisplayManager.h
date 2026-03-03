#pragma once
#include <M5Dial.h>

// ---------------------------------------------------------------------------
// DisplayManager — thin wrapper around the M5Dial display
//
// Provides convenience methods for common drawing operations and manages
// full-screen overlays. Views that need custom drawing (arcs, wheels, etc.)
// should call gfx() for direct M5GFX access rather than going through this
// class.
// ---------------------------------------------------------------------------

class DisplayManager {
public:
  // Initialize display defaults.
  // Sets font to Orbitron_Light_24, datum to middle_center, color to GREEN,
  // and text size to 1. Call once from setup() after M5Dial.begin().
  void begin();

  // Clear the entire display to black.
  void clear();

  // Direct M5GFX access for custom drawing (arcs, wheels, sprites, etc.).
  M5GFX& gfx();

  // Draw text centered on the screen with the specified color and text size.
  // Temporarily overrides the current text size and restores it afterward.
  void drawCenteredText(const char* text, uint16_t color, float textSize = 2.0f);

  // Draw an iOS-style row of page indicator dots centered horizontally.
  // Uses Cfg::DOT_Y_OFFSET, Cfg::DOT_RADIUS_PX, and Cfg::DOT_SPACING_PX.
  //   current  zero-based index of the active page
  //   total    total number of pages
  void drawPageDots(uint8_t current, uint8_t total);

  // Full-screen status overlay.
  // Clears the display and draws centered YELLOW text at the default text size.
  // Used for connectivity status messages such as "Connecting WiFi",
  // "WiFi lost...", and "MQTT...".
  void drawStatusOverlay(const char* message);
};
