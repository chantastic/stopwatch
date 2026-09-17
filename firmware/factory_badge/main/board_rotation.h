// SPDX-License-Identifier: MIT
#pragma once
#include <lvgl.h>
#include <cstdint>

namespace board {
// M5GFX and LVGL number quarter turns in opposite directions. This adapter is
// shared by hardware input, USB input, and tests against the actual LVGL core.
inline lv_display_rotation_t lvRotation(uint8_t m5Rotation) {
    static constexpr lv_display_rotation_t rotations[] = {
        LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_270,
        LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_90
    };
    return rotations[m5Rotation & 3];
}
inline lv_point_t displayToNative(int x, int y, uint8_t rotation, int nativeWidth, int nativeHeight) {
    switch (rotation & 3) {
        case 1: return {nativeWidth - 1 - y, x};
        case 2: return {nativeWidth - 1 - x, nativeHeight - 1 - y};
        case 3: return {y, nativeHeight - 1 - x};
        default: return {x, y};
    }
}
}
