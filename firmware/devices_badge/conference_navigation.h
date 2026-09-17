#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "conference_schedule.h"

enum class ConferencePage : uint8_t { Init, Schedule, AfterDark, Badge, Hack, Settings };
enum class ConferenceGesture : uint8_t { None, Tap, Page, Network, Scroll };

// Positions use the rendered orientation; M5GFX already transforms touch.
class ConferenceNavigation {
 public:
  static constexpr int PAGE_COUNT = 6, NETWORK_COUNT = 3;
  static constexpr int SCHEDULE_ROWS = CONFERENCE_AGENDA_COUNT, ROW_HEIGHT = 78;
  static constexpr int SCROLL_TOP = 130, SCROLL_BOTTOM = 378;
  static constexpr int MAX_SCROLL = SCHEDULE_ROWS * ROW_HEIGHT - (SCROLL_BOTTOM - SCROLL_TOP);
  ConferencePage page = ConferencePage::Init;
  uint8_t network = 0;
  int scroll = 0;
  void nextPage(int direction) {
    page = ConferencePage((int(page) + (direction > 0 ? 1 : PAGE_COUNT - 1)) % PAGE_COUNT);
    cancel();
  }
  void nextNetwork(int direction) {
    network = (network + (direction > 0 ? 1 : NETWORK_COUNT - 1)) % NETWORK_COUNT;
  }
  void begin(int x, int y, uint32_t now) {
    touching_ = true; axis_ = 0; maxTravel_ = 0;
    x_ = x; y_ = y; started_ = now; initialScroll_ = scroll;
  }
  ConferenceGesture move(int x, int y, bool allowNavigation = true) {
    if (!touching_) return ConferenceGesture::None;
    int dx = x - x_, dy = y - y_;
    int ax = abs(dx), ay = abs(dy);
    if (ax + ay > maxTravel_) maxTravel_ = ax + ay;
    if (!axis_ && maxTravel_ >= 12) {
      if (ax * 10 > ay * 13) axis_ = 1;
      else if (ay * 10 > ax * 13) axis_ = 2;
    }
    if (allowNavigation && axis_ == 2 && page == ConferencePage::Schedule) {
      int next = initialScroll_ - dy;
      if (next < 0) next = 0;
      if (next > MAX_SCROLL) next = MAX_SCROLL;
      if (next != scroll) { scroll = next; return ConferenceGesture::Scroll; }
    }
    return ConferenceGesture::None;
  }
  ConferenceGesture end(int x, int y, uint32_t now, bool clickEligible = true, bool allowNavigation = true) {
    if (!touching_) return ConferenceGesture::None;
    move(x, y, allowNavigation); touching_ = false;
    int dx = x - x_, dy = y - y_;
    if (allowNavigation && axis_ == 1 && abs(dx) >= 48) {
      nextPage(dx < 0 ? 1 : -1); return ConferenceGesture::Page;
    }
    if (allowNavigation && axis_ == 2 && page == ConferencePage::Badge && abs(dy) >= 36) {
      nextNetwork(dy < 0 ? 1 : -1); return ConferenceGesture::Network;
    }
    if (!axis_ && maxTravel_ < 8 && uint32_t(now - started_) < 500 && clickEligible)
      return ConferenceGesture::Tap;
    return ConferenceGesture::None;
  }
  void cancel() { touching_ = false; axis_ = 0; }
  bool touching() const { return touching_; }
 private:
  bool touching_ = false;
  int axis_ = 0, x_ = 0, y_ = 0, maxTravel_ = 0, initialScroll_ = 0;
  uint32_t started_ = 0;
};

// Rotation 2 reverses physical left/right. At quarter turns use stock A/B roles.
inline int conferencePusherDirection(bool blue, uint8_t rotation) {
  const bool reversed = rotation == 2;
  return blue ? (reversed ? -1 : 1) : (reversed ? 1 : -1);
}
