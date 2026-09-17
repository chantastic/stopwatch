// SPDX-License-Identifier: MIT
#pragma once
#include "board.h"

namespace board {
// A released marker retains the provenance of its last coordinate. An idle
// controller read is new contact-state information, not a new physical point.
inline void acceptPhysicalSample(TouchSample& sample, bool valid, bool pressed,
                                 int rawX, int rawY, uint32_t now) {
    sample.valid = valid;
    sample.pressed = valid && pressed;
    if (sample.pressed) {
        sample.sensor = true;
        sample.rawX = rawX;
        sample.rawY = rawY;
    }
    sample.sampledAtMs = now;
    ++sample.sequence;
}
inline void acceptInjectedSample(TouchSample& sample, int rawX, int rawY,
                                 int x, int y, bool pressed, uint32_t now) {
    sample.valid = true;
    sample.sensor = false;
    sample.pressed = pressed;
    sample.rawX = rawX;
    sample.rawY = rawY;
    sample.x = x;
    sample.y = y;
    sample.sampledAtMs = now;
    ++sample.sequence;
}
}
