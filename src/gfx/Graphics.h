#pragma once

#include <M5GFX.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Gfx — reusable drawing primitives for the M5Stack Dial UI
//
// Depends on: M5GFX (included via M5Dial.h), Config.h
// All functions take an M5GFX reference so they work with any GFX surface
// (display, sprite, etc.) and are fully decoupled from M5Dial global state.
// ---------------------------------------------------------------------------

namespace Gfx {

// ---------------------------------------------------------------------------
// Color conversion
// ---------------------------------------------------------------------------

// Convert HSV to RGB565. hue is in [0.0, 1.0). sat and val are in [0.0, 1.0].
// Fully saturated / full-brightness (sat=1, val=1) matches the color wheel.
uint16_t hsvToRgb565(float hue, float sat, float val);

// ---------------------------------------------------------------------------
// Color wheel
// ---------------------------------------------------------------------------

// Draw one frame of the spinning rainbow ring.
//
//   angleOffset  rotation offset in degrees (0–359), advances each frame
//   cx, cy       center of the ring in pixels
//   rInner       inner radius of the ring band
//   rOuter       outer radius of the ring band
//
// Each 10-degree segment is colored by its unrotated hue position so the full
// spectrum always covers the ring; only the starting position rotates.
// Matches the visual output of drawColorWheelFrame() in the original .ino.
void drawColorWheel(M5GFX& gfx,
                    int    angleOffset,
                    int    cx,
                    int    cy,
                    int    rInner,
                    int    rOuter);

// ---------------------------------------------------------------------------
// Value arc
// ---------------------------------------------------------------------------

// Draw a partial arc indicating a normalized value in [0.0, 1.0].
//
//   value      0.0 = start of arc, 1.0 = full span
//   color      RGB565 fill color
//   cx, cy     center of arc in pixels
//   radius     outer radius in pixels
//   thickness  radial width of the band in pixels
//   startDeg   angle (degrees) where the arc begins  (default: 135)
//   spanDeg    total sweep (degrees) at value == 1.0  (default: 270)
//
// Caller is responsible for mapping their domain value to [0.0, 1.0] and
// choosing the color. Default angles match the original drawTempArc().
void drawValueArc(M5GFX&   gfx,
                  float    value,
                  uint16_t color,
                  int      cx,
                  int      cy,
                  int      radius,
                  int      thickness,
                  int      startDeg = 135,
                  int      spanDeg  = 270);

// ---------------------------------------------------------------------------
// Page indicator dots
// ---------------------------------------------------------------------------

// Draw an iOS-style row of page indicator dots centered horizontally.
//
//   current    zero-based index of the active page
//   total      total number of pages
//   cy         y coordinate of the dot centers
//   dotRadius  radius of each dot in pixels
//   spacing    center-to-center distance between adjacent dots in pixels
//
// Active dot: WHITE. Inactive dots: dark gray (0x4208).
void drawPageDots(M5GFX&  gfx,
                  uint8_t current,
                  uint8_t total,
                  int     cy,
                  int     dotRadius,
                  int     spacing);

// ---------------------------------------------------------------------------
// Speed bar chart
// ---------------------------------------------------------------------------

// Draw a horizontal row of 6 rectangular speed-indicator bars.
//
//   speed         current fan speed, 0–6. Bars 1..speed are filled; rest are outlines.
//   dirForward    true = forward/up (green→red palette)
//                 false = reverse/down (blue→yellow palette)
//   cx, cy        center of the bar group
//   barW, barH    width and height of each bar in pixels
//   gap           horizontal gap between bars
void drawSpeedBars(M5GFX&  gfx,
                   uint8_t speed,
                   bool    dirForward,
                   int     cx,
                   int     cy,
                   int     barW = 16,
                   int     barH = 28,
                   int     gap  = 4);

// ---------------------------------------------------------------------------
// Arrow
// ---------------------------------------------------------------------------

// Draw a simple directional arrow centered at (cx, cy).
//
//   up     true  → upward-pointing arrow (updraft / Hubspace "reverse")
//          false → downward-pointing arrow (downdraft / Hubspace "forward")
//   color  RGB565 fill color
void drawArrow(M5GFX& gfx, int cx, int cy, bool up, uint16_t color);

}  // namespace Gfx
