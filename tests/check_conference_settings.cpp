#include <cassert>
#include <cstdio>
#include "../firmware/devices_badge/conference_settings.h"
#include "../firmware/devices_badge/conference_navigation.h"
#include "../firmware/devices_badge/orientation_filter.h"

int main() {
  ConferenceSettings settings, restored;
  assert(settings.brightness == 50 && settings.automatic() && !settings.pending());
  assert(!restored.restore(0));
  assert(!restored.restore(0xc701030au));
  assert(!restored.restore(0xc7010000u));
  for (int i=0; i<20; ++i) settings.adjustBrightness(-1, i*20);
  assert(settings.brightness == 10 && settings.displayBrightness() == 26);
  for (int i=0; i<20; ++i) settings.adjustBrightness(1, 1000+i*20);
  assert(settings.brightness == 100 && settings.displayBrightness() == 255);
  assert(!settings.saveDue(2200) && settings.saveDue(2500));
  settings.saved(); assert(!settings.saveDue(99999));
  assert(settings.adjustBrightness(-1, UINT32_MAX-500));
  assert(!settings.saveDue(600) && settings.saveDue(700));
  settings.saveFailed(700); assert(!settings.saveDue(5699) && settings.saveDue(5700));
  settings.saved();
  assert(settings.setOrientation(ConferenceOrientationMode::Default, 100));
  assert(settings.fixedRotation() == 0 && !settings.automatic());
  assert(restored.restore(settings.encoded()) && restored.fixedRotation() == 0 && restored.brightness == 90 && !restored.pending());
  assert(!settings.setOrientation(ConferenceOrientationMode(3), 200));
  assert(settings.setOrientation(ConferenceOrientationMode::Opposite, 200));
  assert(settings.fixedRotation() == 2 && restored.restore(settings.encoded()) && restored.fixedRotation() == 2);
  assert(settings.setOrientation(ConferenceOrientationMode::Free, 300) && settings.automatic());
  assert(conferencePusherDirection(true, 2) == -1 && conferencePusherDirection(false, 2) == 1);
  assert(conferencePusherDirection(true, 0) == 1 && conferencePusherDirection(false, 0) == -1);

  ConferenceNavigation nav; nav.page = ConferencePage::Settings;
  auto before = settings.encoded();
  // No control changes on vertical drags, including a drag that returns to its
  // start. Controls only receive a completed eligible tap from production nav.
  nav.begin(342, 167, 0); nav.move(342, 300); nav.move(342, 167);
  assert(nav.end(342, 167, 250) != ConferenceGesture::Tap && settings.encoded() == before);
  nav.begin(230, 275, 300); nav.move(230, 120);
  assert(nav.end(230, 120, 450) != ConferenceGesture::Tap && nav.page == ConferencePage::Settings);
  nav.begin(342, 167, 500);
  assert(nav.end(342, 167, 600) == ConferenceGesture::Tap);
  assert(conferenceSettingsHit(342, 167) == ConferenceSettingsAction::Brighter);
  assert(conferenceSettingsHit(126, 167) == ConferenceSettingsAction::Dimmer);
  assert(conferenceSettingsHit(234, 272) == ConferenceSettingsAction::Connect);
  assert(conferenceSettingsHit(136, 362) == ConferenceSettingsAction::Free);
  assert(conferenceSettingsHit(234, 362) == ConferenceSettingsAction::Default);
  assert(conferenceSettingsHit(332, 362) == ConferenceSettingsAction::Opposite);
  assert(conferenceSettingsHit(234, 400) == ConferenceSettingsAction::None);
  assert(conferenceSettingsHit(234, 330) == ConferenceSettingsAction::None);

  OrientationFilter orientation;
  orientation.reset(2);
  for (uint32_t t=0;t<=1000;t+=50) assert(!orientation.update(0,-1,0,t,true));
  assert(orientation.rotation() == 2);
  assert(!orientation.update(0,-1,0,1050,false));
  for (uint32_t t=1100;t<1750;t+=50) assert(!orientation.update(0,-1,0,t,false));
  assert(orientation.update(0,-1,0,1750,false) && orientation.rotation() == 0);
  // Seed Free from the fixed display pose; all four automatic directions still
  // settle from fresh readings, without inheriting an old pending candidate.
  const float vectors[4][2]={{0,-1},{1,0},{0,1},{-1,0}};
  for (int target=0;target<4;++target) {
    orientation.reset((target+1)%4);
    for (uint32_t t=0;t<=700;t+=50) orientation.update(vectors[target][0],vectors[target][1],0,t,false);
    assert(orientation.rotation() == target);
  }

  constexpr int64_t midnight = 1704153600; // Synthetic 2024-01-02 00:00 UTC.
  const ConferenceScheduleItem items[] = {
    {"untimed", "", 0, 0}, {"before midnight", "", midnight-600, midnight},
    {"after midnight", "", midnight, midnight+600}, {"later", "", midnight+1200, midnight+1800},
    {"invalid", "", midnight+3000, midnight+2000},
  };
  assert(conferenceCurrentScheduleItem(items, 5, midnight-601, true) == -1);
  assert(conferenceCurrentScheduleItem(items, 5, midnight-600, true) == 1);
  assert(conferenceCurrentScheduleItem(items, 5, midnight-1, true) == 1);
  assert(conferenceCurrentScheduleItem(items, 5, midnight, true) == 2);
  assert(conferenceCurrentScheduleItem(items, 5, midnight+600, true) == -1);
  assert(conferenceCurrentScheduleItem(items, 5, midnight+1200, true) == 3);
  assert(conferenceCurrentScheduleItem(items, 5, midnight+1800, true) == -1);
  assert(conferenceCurrentScheduleItem(items, 5, midnight, false) == -1);
  assert(conferenceCurrentScheduleItem(CONFERENCE_AGENDA, CONFERENCE_AGENDA_COUNT, midnight, true) == -1);
  // Forward/backward clock corrections select solely by current absolute UTC;
  // the display offset and the user's scroll position are not inputs.
  nav.page=ConferencePage::Schedule; nav.scroll=91;
  assert(conferenceCurrentScheduleItem(items, 5, midnight+1250, true) == 3);
  assert(conferenceCurrentScheduleItem(items, 5, midnight-100, true) == 1);
  assert(nav.scroll == 91);
  std::puts("Conference settings: bounded brightness, coalesced preferences, exact orientation modes, release/drag safety, UTC schedule boundaries and corrections passed");
}
