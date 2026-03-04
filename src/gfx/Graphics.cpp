#include "Graphics.h"

// ---------------------------------------------------------------------------
// Gfx — implementation
// ---------------------------------------------------------------------------

namespace Gfx {

// ---------------------------------------------------------------------------
// hsvToRgb565
//
// Extracted verbatim from drawColorWheelFrame() in the original .ino.
// sat and val parameters are accepted for generality but the color wheel
// always calls this with sat=1, val=1, which matches the original's hardcoded
// v=255, p=0 path (p = v*(1-s) = 0 when s=1).
// ---------------------------------------------------------------------------
uint16_t hsvToRgb565(float hue, float sat, float val) {
  // Clamp inputs
  if (hue  < 0.0f) hue  = 0.0f;
  if (hue  >= 1.0f) hue = 0.0f;   // wrap 1.0 back to 0.0 (red)
  if (sat  < 0.0f) sat  = 0.0f;
  if (sat  > 1.0f) sat  = 1.0f;
  if (val  < 0.0f) val  = 0.0f;
  if (val  > 1.0f) val  = 1.0f;

  uint8_t r, g, b;

  int   h = int(hue * 6);
  float f = hue * 6 - h;

  uint8_t v = uint8_t(val * 255);
  uint8_t p = uint8_t(v * (1.0f - sat));
  uint8_t q = uint8_t(v * (1.0f - f * sat));
  uint8_t t = uint8_t(v * (1.0f - (1.0f - f) * sat));

  switch (h % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = 0; g = 0; b = 0; break;
  }

  // Pack to RGB565: R[4:0] G[5:0] B[4:0]
  return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// ---------------------------------------------------------------------------
// drawColorWheel
//
// Port of drawColorWheelFrame() from the original .ino.
//
// The original draws 36 segments of 10 degrees each.  For each segment at
// unrotated position i (0, 10, 20, … 350), the hue is derived from i alone
// (hue = i / 360.0), so the full spectrum always covers the ring regardless
// of angleOffset.  The offset only shifts *where* each hue is painted.
//
// Wrap-around handling: when a rotated segment straddles 360°, the original
// splits it into two fillArc calls (angleStart→360, then 0→angleEnd).
// That behavior is preserved here exactly.
// ---------------------------------------------------------------------------
void drawColorWheel(M5GFX& gfx,
                    int    angleOffset,
                    int    cx,
                    int    cy,
                    int    rInner,
                    int    rOuter) {
  for (int i = 0; i < 360; i += 10) {
    int angleStart = (i + angleOffset) % 360;
    int angleEnd   = (i + 10 + angleOffset) % 360;

    // Hue is based on the unrotated position, matching the original exactly.
    float hue = i / 360.0f;

    // Replicate the original's hardcoded sat=1, val=1 path:
    //   v=255, p=0 (since p = v*(1-sat) = 0), q=255*(1-f), t=255*f
    int   h = int(hue * 6);
    float f = hue * 6 - h;

    uint8_t v = 255;
    uint8_t p = 0;
    uint8_t q = uint8_t(255.0f * (1.0f - f));
    uint8_t t = uint8_t(255.0f * f);

    uint8_t r, g, b;
    switch (h % 6) {
      case 0: r = v; g = t; b = p; break;
      case 1: r = q; g = v; b = p; break;
      case 2: r = p; g = v; b = t; break;
      case 3: r = p; g = q; b = v; break;
      case 4: r = t; g = p; b = v; break;
      case 5: r = v; g = p; b = q; break;
      default: r = 0; g = 0; b = 0; break;
    }

    uint16_t color = gfx.color565(r, g, b);

    // Draw — split across 360° boundary exactly as the original does.
    if (angleEnd < angleStart) {
      gfx.fillArc(cx, cy, rInner, rOuter, angleStart, 360, color);
      gfx.fillArc(cx, cy, rInner, rOuter, 0, angleEnd, color);
    } else {
      gfx.fillArc(cx, cy, rInner, rOuter, angleStart, angleEnd, color);
    }
  }
}

// ---------------------------------------------------------------------------
// drawValueArc
//
// Generalized port of drawTempArc() from the original .ino.
//
// The original:
//   1. Clamps tempF to [90, 140]
//   2. Maps the clamped value to [0, 270] degrees
//   3. Draws fillArc(cx, cy, radius-thickness, radius, 135, 135+angle, color)
//
// Here the caller is responsible for clamping and mapping their domain value
// to [0.0, 1.0], and for selecting the color.  The arc geometry (startDeg,
// spanDeg, radius, thickness) is fully parameterized.
//
// When called with the defaults (startDeg=135, spanDeg=270) and a value
// computed as:
//   value = (clamped - 90) / 50.0f
// …and with the same color logic as the original, the output is pixel-identical
// to drawTempArc().
// ---------------------------------------------------------------------------
void drawValueArc(M5GFX&   gfx,
                  float    value,
                  uint16_t color,
                  int      cx,
                  int      cy,
                  int      radius,
                  int      thickness,
                  int      startDeg,
                  int      spanDeg) {
  if (value <= 0.0f) return;   // nothing to draw at zero
  if (value >  1.0f) value = 1.0f;

  int angle = int(value * spanDeg);

  // fillArc(cx, cy, innerRadius, outerRadius, startAngle, endAngle, color)
  gfx.fillArc(cx, cy,
               radius - thickness, radius,
               startDeg, startDeg + angle,
               color);
}

// ---------------------------------------------------------------------------
// drawPageDots
//
// Draw a centered row of filled circles representing page position.
// Active dot is WHITE; inactive dots are dark gray (0x4208 ≈ #424242 in 5-6-5).
// ---------------------------------------------------------------------------
void drawPageDots(M5GFX&  gfx,
                  uint8_t current,
                  uint8_t total,
                  int     cy,
                  int     dotRadius,
                  int     spacing) {
  if (total == 0) return;

  // Total width of the dot row: (total-1) gaps of `spacing` plus two half-dot
  // radii at each end.  Center the row horizontally.
  int rowWidth = (total - 1) * spacing;
  int startX   = (gfx.width() - rowWidth) / 2;

  constexpr uint16_t ACTIVE_COLOR   = 0xFFFF;   // WHITE
  constexpr uint16_t INACTIVE_COLOR = 0x4208;   // dark gray

  for (uint8_t i = 0; i < total; i++) {
    int x = startX + i * spacing;
    uint16_t color = (i == current) ? ACTIVE_COLOR : INACTIVE_COLOR;
    gfx.fillCircle(x, cy, dotRadius, color);
  }
}

// ---------------------------------------------------------------------------
// drawSpeedBars
//
// Six vertical bars representing fan speed 1–6. Bars up to the current speed
// are filled with a direction-dependent color gradient; remaining bars are
// drawn as dark gray outlines.
//
// Forward/UP:   green (speed 1) → red (speed 6)    HSV hue 0.333 → 0.0
// Reverse/DOWN: blue (speed 1) → yellow (speed 6)  HSV hue 0.667 → 0.167
// ---------------------------------------------------------------------------
void drawSpeedBars(M5GFX&  gfx,
                   uint8_t speed,
                   bool    dirForward,
                   int     cx,
                   int     cy,
                   int     barW,
                   int     barH,
                   int     gap) {
  constexpr int N = 6;
  int totalW = N * barW + (N - 1) * gap;
  int x0 = cx - totalW / 2;
  int y0 = cy - barH / 2;

  for (int i = 0; i < N; i++) {
    int x = x0 + i * (barW + gap);
    bool filled = (speed > 0) && (i < static_cast<int>(speed));

    if (filled) {
      float hue;
      if (dirForward) {
        hue = 0.333f * (1.0f - float(i) / float(N - 1));
      } else {
        hue = 0.667f - float(i) / float(N - 1) * 0.5f;
      }
      uint16_t color = hsvToRgb565(hue, 1.0f, 1.0f);
      gfx.fillRect(x, y0, barW, barH, color);
    } else {
      constexpr uint16_t OUTLINE = 0x4208;
      gfx.drawRect(x, y0, barW, barH, OUTLINE);
    }
  }
}

// ---------------------------------------------------------------------------
// drawArrow
//
// Draw a simple filled triangle arrow centered at (cx, cy).
// The arrowhead points up or down; a short shaft is drawn below/above it.
// Sized to be legible at the M5Dial's 240×240 resolution.
// ---------------------------------------------------------------------------
void drawArrow(M5GFX& gfx, int cx, int cy, bool up, uint16_t color) {
  // Head: equilateral-ish triangle, 24px wide × 16px tall
  constexpr int headW = 12;   // half-width of the triangle base
  constexpr int headH = 16;   // height of the triangle

  // Shaft: 6px wide × 12px tall, centered under/above the head
  constexpr int shaftW = 3;   // half-width of the shaft
  constexpr int shaftH = 12;

  if (up) {
    // Tip at top, base at bottom
    int tipY  = cy - headH / 2;
    int baseY = cy + headH / 2;

    gfx.fillTriangle(cx,          tipY,
                     cx - headW,  baseY,
                     cx + headW,  baseY,
                     color);

    // Shaft below the base
    gfx.fillRect(cx - shaftW, baseY, shaftW * 2, shaftH, color);
  } else {
    // Tip at bottom, base at top
    int tipY  = cy + headH / 2;
    int baseY = cy - headH / 2;

    gfx.fillTriangle(cx,          tipY,
                     cx - headW,  baseY,
                     cx + headW,  baseY,
                     color);

    // Shaft above the base
    gfx.fillRect(cx - shaftW, baseY - shaftH, shaftW * 2, shaftH, color);
  }
}

}  // namespace Gfx
