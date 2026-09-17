#pragma once
#include <cstdint>
class Preferences {
 public:
  inline static bool available = true, writable = true, stored = false;
  inline static int value = 0;
  bool begin(const char*,bool) { return available; }
  bool isKey(const char*) { return stored; }
  int getInt(const char*,int fallback) { return stored ? value : fallback; }
  size_t putInt(const char*,int v) { if (!writable) return 0; value=v;stored=true;return sizeof(int32_t); }
  void end() {}
};
