#pragma once
#include <cstdint>

namespace Cfg {

  // --- Display geometry ---
  constexpr int DISPLAY_CX = 120;          // horizontal center of the 240x240 round LCD
  constexpr int DISPLAY_CY = 120;          // vertical center

  // --- Temperature arc ---
  constexpr int   ARC_RADIUS    = 100;     // outer radius of the temperature arc (px)
  constexpr int   ARC_THICKNESS = 10;      // radial thickness (px)
  constexpr int   ARC_START_DEG = 135;     // start angle in degrees (7 o'clock position)
  constexpr int   ARC_SPAN_DEG  = 270;     // total arc sweep in degrees
  constexpr float TEMP_MIN_F    = 90.0f;   // temperature that maps to arc start
  constexpr float TEMP_MAX_F    = 140.0f;  // temperature that maps to arc end

  // --- Color wheel ---
  constexpr int CW_INNER_RADIUS  = 83;     // inner radius of the spinning color ring (px)
  constexpr int CW_OUTER_RADIUS  = 87;     // outer radius of the spinning color ring (px)
  constexpr int CW_STEP_DEG      = 10;     // angular width of each color segment (degrees)
  constexpr int CW_ADVANCE_DEG   = 8;      // rotation advance per animation frame (degrees)

  // --- Animation ---
  constexpr unsigned long FRAME_INTERVAL_MS = 50;   // ~20 fps

  // --- WiFi ---
  constexpr unsigned long WIFI_CHECK_INTERVAL_MS = 5000;  // how often to poll WiFi status

  // --- MQTT retry / backoff ---
  constexpr unsigned long MQTT_RETRY_MIN_MS = 2000;   // initial retry interval
  constexpr unsigned long MQTT_RETRY_MAX_MS = 30000;  // backoff ceiling

  // --- Navigation ---
  constexpr int SWIPE_THRESHOLD_PX = 40;  // minimum swipe distance to register a page change
  constexpr int MAX_VIEWS          = 8;   // total number of navigable views

  // --- Page indicator dots ---
  constexpr int DOT_RADIUS_PX  = 4;    // radius of each page-indicator dot (px)
  constexpr int DOT_Y_OFFSET   = 220;  // y position of the dot row
  constexpr int DOT_SPACING_PX = 14;   // center-to-center spacing between dots

}  // namespace Cfg
