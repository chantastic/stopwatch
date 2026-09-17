#pragma once
#include <math.h>
#include <stdint.h>

struct ConferenceTouchPoint { int x, y; };

// Global scale/offset trial fitted to equally weighted medians from five target
// regions on September 17. See docs/hardware.md for evidence and limitations.
// Input is already transformed by M5GFX. Do not apply this to synthetic UI taps.
// No target snapping, edge clamping, or per-button adjustments.
inline ConferenceTouchPoint conferenceScaleTouch(int x, int y, uint8_t rotation) {
  constexpr float sx = 0.82758047844f, ox = 35.54620127f;
  constexpr float sy = 0.85758081377f, oy = 14.17202075f;
  constexpr float lastPixel = 467.0f;
  constexpr float oppositeX = lastPixel * (1.0f - sx) - ox;
  constexpr float oppositeY = lastPixel * (1.0f - sy) - oy;
  // Rotate the correction axes/origin, not the already-rotated touch point.
  // This keeps the same physical sensor correction in all four orientations.
  switch (rotation & 3) {
    case 1: return {int(lroundf(sy*x + oy)), int(lroundf(sx*y + oppositeX))};
    case 2: return {int(lroundf(sx*x + oppositeX)), int(lroundf(sy*y + oppositeY))};
    case 3: return {int(lroundf(sy*x + oppositeY)), int(lroundf(sx*y + ox))};
    default: return {int(lroundf(sx*x + ox)), int(lroundf(sy*y + oy))};
  }
}
