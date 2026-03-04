#pragma once
#include "../DeviceView.h"
#include "../Config.h"
#include <M5Dial.h>
#include <cstdint>

class DisplayManager;

class Navigator {
public:
  void setViews(DeviceView** views, uint8_t count);

  // Process touch input for swipe gestures. Call before pollInput() each loop.
  // Touch detail is read once in loop() and passed in to avoid consuming one-shot flags.
  void processTouchInput(const m5::touch_detail_t& touch, DisplayManager& display);

  DeviceView* activeView() const;
  uint8_t     activeIndex() const;
  uint8_t     viewCount() const;

  // Programmatic navigation
  void goTo(uint8_t index, DisplayManager& display);

private:
  DeviceView** _views   = nullptr;
  uint8_t      _count   = 0;
  uint8_t      _current = 0;

  void switchTo(uint8_t index, DisplayManager& display);
};
