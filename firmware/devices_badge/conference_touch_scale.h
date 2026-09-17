#pragma once
#include <math.h>
#include <stdint.h>

struct ConferenceTouchPoint { int x, y; };
constexpr const char *CONFERENCE_TOUCH_MODEL = "scale-offset-2";

// Continuous trial: retain the five-region scale fit, then separate the native
// and screen-relative residuals measured across four orientations on Sept 17.
// These describe measured coordinate frames, not established physical causes.
// See docs/hardware.md for the fit and its center-only validation limitations.
// Input is already transformed by M5GFX. Do not apply this to synthetic UI taps.
// No target snapping, edge clamping, or per-button adjustments.
inline ConferenceTouchPoint conferenceScaleTouch(int x, int y, uint8_t rotation) {
  constexpr float sx = 0.82758047844f, sy = 0.85758081377f;
  constexpr float nativeResidualX = -1.75f, nativeResidualY = -10.75f;
  constexpr float screenResidualX = 2.5f, screenResidualY = 17.0f;
  constexpr float ox = 35.54620127f - nativeResidualX;
  constexpr float oy = 14.17202075f - nativeResidualY;
  constexpr float lastPixel = 467.0f;
  constexpr float oppositeX = lastPixel * (1.0f - sx) - ox;
  constexpr float oppositeY = lastPixel * (1.0f - sy) - oy;
  // Rotate the native correction axes/origin, not the already-rotated input.
  // The screen-relative correction stays in the readable UI's coordinate frame.
  float correctedX, correctedY;
  switch (rotation & 3) {
    case 1: correctedX = sy*x + oy; correctedY = sx*y + oppositeX; break;
    case 2: correctedX = sx*x + oppositeX; correctedY = sy*y + oppositeY; break;
    case 3: correctedX = sy*x + oppositeY; correctedY = sx*y + ox; break;
    default: correctedX = sx*x + ox; correctedY = sy*y + oy; break;
  }
  // Round once, after both frames' corrections; never snap or clamp the result.
  return {int(lroundf(correctedX - screenResidualX)),
          int(lroundf(correctedY - screenResidualY))};
}
