#pragma once

#include <cstdint>

// Input event types dispatched to device views.
//
// Swipe gestures are NOT included here — they are consumed by the Navigator
// before reaching views.
enum class InputType : uint8_t {
    None,
    ButtonPress,
    ButtonDoubleClick,
    ButtonHold,
    EncoderDelta,     // delta field = encoder ticks since last read
    TouchTap,
    TouchHoldStart,
    TouchHoldEnd,
};

struct InputEvent {
    InputType type  = InputType::None;
    int16_t   delta = 0;  // encoder delta (only meaningful for EncoderDelta)
};
