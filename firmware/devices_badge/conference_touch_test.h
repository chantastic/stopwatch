#pragma once
#include <stdint.h>

// A temporary observation tool. It never changes calibration or preferences.
class ConferenceTouchTest {
 public:
  bool active = false, pressed = false, hasSample = false, sensorSample = false;
  int x = 0, y = 0, rawX = 0, rawY = 0;
  uint8_t rotation = 0;
  void open(uint8_t pose) {
    active = true; pressed = hasSample = sensorSample = false;
    x = y = rawX = rawY = 0; rotation = pose;
  }
  void close() { active = pressed = false; }
  bool observe(int screenX, int screenY, bool down, bool sensor = false, int rawScreenX = 0, int rawScreenY = 0) {
    if (!active) return false;
    bool changed = !hasSample || pressed != down || x != screenX || y != screenY || sensorSample != sensor;
    if (sensor && (rawX != rawScreenX || rawY != rawScreenY)) changed = true;
    x = screenX; y = screenY; pressed = down; sensorSample = sensor; hasSample = true;
    rawX = rawScreenX; rawY = rawScreenY;
    return changed;
  }
  bool release() {
    if (!active || !pressed) return false;
    pressed = false; return true;
  }
};
