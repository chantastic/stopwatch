#include <cassert>
#include <cstdio>
#include "../firmware/devices_badge/conference_navigation.h"
int main() {
  ConferenceNavigation n;
  for (int i = 1; i <= 6; ++i) { n.nextPage(1); assert(int(n.page) == i % 6); }
  for (int i = 5; i >= 0; --i) { n.nextPage(-1); assert(int(n.page) == i); }
  n.page = ConferencePage::Badge;
  n.begin(234, 300, 0); n.move(236, 220);
  assert(n.end(236, 200, 200) == ConferenceGesture::Network);
  assert(n.network == 1 && n.page == ConferencePage::Badge);
  n.begin(230, 280, 300); n.move(231, 200); n.move(230, 280);
  assert(n.end(230, 280, 450) == ConferenceGesture::None);
  n.begin(210, 250, 500); assert(n.end(211, 250, 600) == ConferenceGesture::Tap);
  n.begin(210, 250, 700); assert(n.end(211, 250, 1300) == ConferenceGesture::None);
  n.begin(210, 250, 1400); assert(n.end(210, 250, 1410, false) == ConferenceGesture::None);
  for (int i=0; i<3; ++i) n.nextNetwork(1);
  assert(n.network == 1);
  n.page = ConferencePage::Schedule;
  n.begin(240, 350, 0); n.move(240, -500);
  assert(n.scroll == ConferenceNavigation::MAX_SCROLL);
  assert(ConferenceNavigation::SCROLL_TOP + ConferenceNavigation::SCHEDULE_ROWS * ConferenceNavigation::ROW_HEIGHT - n.scroll == ConferenceNavigation::SCROLL_BOTTOM);
  n.end(240, -500, 100);
  n.begin(240, 150, 200); n.move(240, 900); assert(n.scroll == 0); n.end(240, 900, 300);
  n.begin(300, 240, 500); assert(n.end(100, 242, 700) == ConferenceGesture::Page);
  assert(n.page == ConferencePage::AfterDark);
  n.begin(200, 200, 0); n.cancel(); assert(n.end(200, 200, 10) == ConferenceGesture::None);
  assert(conferencePusherDirection(true, 2) == -1);
  assert(conferencePusherDirection(false, 2) == 1);
  assert(conferencePusherDirection(true, 0) == 1);
  n.page = ConferencePage::Schedule; n.scroll = 0;
  n.begin(230, 350, 0); n.move(230, 100, false);
  assert(n.end(230, 100, 100, true, false) == ConferenceGesture::None && n.scroll == 0);
  n.page = ConferencePage::Badge; const auto savedNetwork = n.network;
  n.begin(230, 350, 0); n.move(230, 100, false);
  assert(n.end(230, 100, 100, true, false) == ConferenceGesture::None && n.network == savedNetwork);
  n.begin(340, 240, 0); n.move(100, 240, false);
  assert(n.end(100, 240, 100, true, false) == ConferenceGesture::None && n.page == ConferencePage::Badge);
  n.begin(230, 420, 0); n.move(230, 200, false); n.move(230, 420, false);
  assert(n.end(230, 420, 100, true, false) == ConferenceGesture::None);
  std::puts("Conference navigation: page order, network isolation, scroll bounds and gesture arbitration passed");
}
