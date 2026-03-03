#include "Navigator.h"
#include "DisplayManager.h"
#include <M5Dial.h>

// ---------------------------------------------------------------------------
// setViews
// ---------------------------------------------------------------------------

void Navigator::setViews(DeviceView** views, uint8_t count) {
  _views   = views;
  _count   = count;
  _current = 0;
}

// ---------------------------------------------------------------------------
// processTouchInput
//
// Reads the M5Dial touch subsystem directly and consumes flick gestures for
// page navigation. Swipe left (negative distanceX) advances to the next view;
// swipe right (positive distanceX) retreats to the previous view. Both
// directions wrap around.
// ---------------------------------------------------------------------------

void Navigator::processTouchInput(DisplayManager& display) {
  if (_views == nullptr || _count == 0) return;

  auto t = M5Dial.Touch.getDetail();

  if (!t.wasFlicked()) return;

  if (t.distanceX() < -Cfg::SWIPE_THRESHOLD_PX) {
    // Swipe left — next view, wrap to 0 at end
    uint8_t next = (_current + 1 < _count) ? _current + 1 : 0;
    switchTo(next, display);
  } else if (t.distanceX() > Cfg::SWIPE_THRESHOLD_PX) {
    // Swipe right — previous view, wrap to last at 0
    uint8_t prev = (_current > 0) ? _current - 1 : _count - 1;
    switchTo(prev, display);
  }
}

// ---------------------------------------------------------------------------
// goTo — public programmatic navigation
// ---------------------------------------------------------------------------

void Navigator::goTo(uint8_t index, DisplayManager& display) {
  switchTo(index, display);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

DeviceView* Navigator::activeView() const {
  return (_views != nullptr && _count > 0) ? _views[_current] : nullptr;
}

uint8_t Navigator::activeIndex() const {
  return _current;
}

uint8_t Navigator::viewCount() const {
  return _count;
}

// ---------------------------------------------------------------------------
// switchTo — internal; notifies outgoing and incoming views
// ---------------------------------------------------------------------------

void Navigator::switchTo(uint8_t index, DisplayManager& display) {
  if (index == _current) return;

  _views[_current]->onDeactivate();
  _current = index;
  _views[_current]->onActivate(display);
}
