#pragma once
#include "../DeviceView.h"
#include "../Config.h"
#include <cstdint>

class DisplayManager;

class Navigator {
public:
  void setViews(DeviceView** views, uint8_t count);

  // Process touch input for swipe gestures. Call before pollInput() each loop.
  // Reads M5Dial.Touch.getDetail() directly and consumes flick gestures.
  // Needs DisplayManager reference to trigger onActivate/onDeactivate on view switch.
  void processTouchInput(DisplayManager& display);

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
