#pragma once
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_bt.h>
#include <algorithm>
#include "orientation_filter.h"
#include "button_gesture.h"
#include "conference_navigation.h"
#include "conference_settings.h"
#include "conference_touch_test.h"
#include "conference_touch_scale.h"
#include "conference_profile_store.h"
#include "conference_portal.h"
#include "conference_clock.h"
#include "init_wordmark.h"
#include "github_mark.h"

SET_LOOP_TASK_STACK_SIZE(32768);
static constexpr char CONFERENCE_BUILD[] = "conference-settings-2";
static constexpr char HACK_URL[] = "https://drop.workos.cloud/stopwatch";
static constexpr uint16_t DIM = 0x9CF3, PANEL = 0x18C3, ACCENT = 0xC4FF;
static constexpr const char *PAGE_NAMES[] = {"init()", "Schedule", "After Dark", "Badge", "Hack your Badge", "Settings"};
static constexpr const char *NETWORK_NAMES[] = {"GitHub", "X / Twitter", "LinkedIn"};
ConferenceNavigation navigation;
ConferenceSettings badgeSettings;
ConferenceTouchTest touchTest;
ConferencePage conferenceSetupReturn = ConferencePage::Badge;
bool conferenceOrientationPending = false, conferencePreferencesReady = false;
bool conferenceSuppressTouchUntilRelease = false;
uint32_t conferencePreferenceWrites = 0, conferenceLastPowerRead = 0;
int conferenceCurrentItem = -1;
ConferenceProfileStore manualProfile;
ConferencePortal manualPortal(manualProfile);
OrientationFilter conferenceOrientation;
BadgeButtonGesture conferenceButtons;
Preferences conferenceUi;
bool conferenceRedraw = true, conferenceSetup = false, conferenceExpanded = false;
bool conferenceSerialOverflow = false;
String conferenceSerial, conferenceToast;
uint32_t conferenceLastFrame = 0, conferenceLastImu = 0, conferenceLastStatus = 0;
uint32_t conferenceToastUntil = 0, conferenceSaveSelectionAt = 0, conferenceInputCount = 0;
uint32_t conferenceLastLoop = 0, conferenceMaxGap = 0, conferenceFrames = 0;
uint32_t conferenceProfileRevision = 0;
String conferenceLastClock;

static bool conferenceDue(uint32_t time) { return int32_t(millis() - time) >= 0; }
static String conferenceFit(String text, int width) {
  auto &d = M5.Display;
  if (d.textWidth(text) <= width) return text;
  while (text.length() && d.textWidth(text + "...") > width) {
    int cut = text.length() - 1;
    while (cut > 0 && (uint8_t(text[cut]) & 0xc0) == 0x80) --cut;
    text.remove(cut);
  }
  return text + "...";
}
static void conferenceText(const String &s, int x, int y, const lgfx::IFont *font = &fonts::FreeSans9pt7b, uint16_t color = TFT_WHITE, int width = 330, uint16_t background = TFT_BLACK) {
  auto &d = M5.Display;
  d.setFont(font); d.setTextSize(1); d.setTextColor(color, background); d.setTextDatum(middle_center);
  d.drawString(conferenceFit(s, width), x, y);
}
static void conferenceNotice(const String &s) { conferenceToast = s; conferenceToastUntil = millis() + 4000; conferenceRedraw = true; }
static void conferenceRememberNetwork() { conferenceSaveSelectionAt = millis() + 1200; }

// Network marks remain tied to the exact slot used for the URL and label.
static void conferenceNetworkIcon(uint8_t network, int x, int y, int size) {
  auto &d = M5.Display;
  if (network == 0) { drawGitHub(x, y, size); return; }
  if (network == 2) {
    d.fillRoundRect(x, y, size, size, 3, TFT_WHITE);
    d.setFont(&fonts::FreeSansBold12pt7b); d.setTextSize(1);
    d.setTextColor(TFT_BLACK, TFT_WHITE); d.setTextDatum(middle_center);
    d.drawString("in", x + size / 2, y + size / 2 - 1); return;
  }
  // Two diagonals with the distinct broad outlined X stroke.
  d.fillTriangle(x, y, x + size/4, y, x + size, y + size, TFT_WHITE);
  d.fillTriangle(x, y, x + size*3/4, y + size, x + size, y + size, TFT_WHITE);
  d.fillTriangle(x + size/10, y + size/15, x + size/5, y + size/15, x + size*9/10, y + size*14/15, TFT_BLACK);
  d.fillTriangle(x + size/10, y + size/15, x + size*4/5, y + size*14/15, x + size*9/10, y + size*14/15, TFT_BLACK);
  d.drawWideLine(x + size - 1, y, x, y + size - 1, 2, TFT_WHITE);
}
static void conferenceAvatar(int left, int top, int size) {
  auto &d = M5.Display;
  const auto *pixels = manualProfile.avatarPixels();
  const int w = manualProfile.avatarWidth(), h = manualProfile.avatarHeight();
  if (!pixels || w < 1 || h < 1) {
    d.fillCircle(left + size/2, top + size/2, size/2, PANEL);
    d.fillCircle(left + size/2, top + size*3/8, size/6, DIM);
    d.fillRoundRect(left + size/5, top + size*3/5, size*3/5, size/4, size/8, DIM);
    return;
  }
  for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
    int dx = x - size/2, dy = y - size/2;
    if (dx*dx + dy*dy <= size*size/4)
      d.drawPixel(left + x, top + y, pixels[(y*h/size)*w + x*w/size]);
  }
}
static void conferenceChrome() {
  auto &d = M5.Display;
  conferenceText(conferenceClockText(), 234, 36, &fonts::FreeSans9pt7b, DIM, 120);
  d.drawWideLine(42, 222, 30, 234, 3, TFT_WHITE);
  d.drawWideLine(30, 234, 42, 246, 3, TFT_WHITE);
  d.drawWideLine(426, 222, 438, 234, 3, TFT_WHITE);
  d.drawWideLine(438, 234, 426, 246, 3, TFT_WHITE);
  const bool notice = conferenceToast.length() && !conferenceDue(conferenceToastUntil);
  conferenceText(notice ? conferenceToast : badgeSettings.pending() && navigation.page == ConferencePage::Settings ? String("Saving settings...") : String(PAGE_NAMES[int(navigation.page)]), 234, 416,
    &fonts::FreeSans9pt7b, notice ? ACCENT : TFT_WHITE, 270);
  for (int i = 0; i < ConferenceNavigation::PAGE_COUNT; ++i) d.fillCircle(194 + i*16, 442, i == int(navigation.page) ? 4 : 3, i == int(navigation.page) ? TFT_WHITE : 0x4208);
}
static void conferenceInit() {
  auto &d = M5.Display;
  float phase = float(millis() % 4800) * (6.2831853f / 4800.0f);
  for (int i = 0; i < 24; ++i) {
    float angle = float(i) * 6.2831853f/24;
    uint8_t shade = uint8_t(65 + 80 * (1 + cosf(angle - phase)));
    d.fillCircle(234 + int(128*cosf(angle)), 217 + int(128*sinf(angle)), 2 + (i % 3 == 0), d.color565(shade, shade, shade));
  }
  drawInitWordmark(106, 188, 256, TFT_WHITE);
  conferenceText("CONFERENCE BADGE", 234, 270, &fonts::FreeSans9pt7b, DIM);
  conferenceText("Animation preview", 234, 364, &fonts::FreeSans9pt7b, DIM);
  int battery = M5.Power.getBatteryLevel();
  d.drawRoundRect(296, 51, 24, 12, 2, DIM); d.fillRect(320, 55, 3, 4, DIM);
  if (battery >= 0) d.fillRect(299, 54, 18 * min(100, battery)/100, 6, TFT_WHITE);
  conferenceText(battery < 0 ? String("--") : String(battery) + "%", 275, 58, &fonts::Font0, DIM, 38);
}
static void conferenceSchedule() {
  auto &d = M5.Display;
  conferenceText("Schedule", 234, 83, &fonts::FreeSansBold18pt7b);
  conferenceText("Preview / event details pending", 234, 110, &fonts::FreeSans9pt7b, DIM);
  d.setClipRect(80, ConferenceNavigation::SCROLL_TOP, 308, ConferenceNavigation::SCROLL_BOTTOM - ConferenceNavigation::SCROLL_TOP);
  for (int i = 0; i < ConferenceNavigation::SCHEDULE_ROWS; ++i) {
    int y = ConferenceNavigation::SCROLL_TOP + i*ConferenceNavigation::ROW_HEIGHT - navigation.scroll;
    bool current = i == conferenceCurrentItem;
    d.fillRoundRect(88, y + 3, 288, 68, 10, PANEL);
    if (current) { d.drawRoundRect(88, y + 3, 288, 68, 10, ACCENT); d.fillRoundRect(89, y + 16, 4, 42, 2, ACCENT); }
    conferenceText(CONFERENCE_AGENDA[i].title, 232, y + 26, &fonts::FreeSansBold12pt7b, current ? ACCENT : TFT_WHITE, 270);
    conferenceText(current ? String("Now / ") + CONFERENCE_AGENDA[i].detail : String(CONFERENCE_AGENDA[i].detail), 232, y + 52, &fonts::FreeSans9pt7b, DIM, 270);
  }
  d.clearClipRect();
  int track = ConferenceNavigation::SCROLL_BOTTOM - ConferenceNavigation::SCROLL_TOP;
  d.fillRoundRect(385, ConferenceNavigation::SCROLL_TOP, 3, track, 1, PANEL);
  d.fillRoundRect(385, ConferenceNavigation::SCROLL_TOP + navigation.scroll*(track-40)/ConferenceNavigation::MAX_SCROLL, 3, 40, 1, DIM);
  conferenceText("Swipe up / down", 234, 392, &fonts::Font0, DIM);
}
static void conferenceAfterDark() {
  auto &d = M5.Display;
  drawInitWordmark(184, 76, 100, DIM);
  conferenceText("Developers", 234, 144, &fonts::FreeSansBold18pt7b);
  conferenceText("After Dark", 234, 184, &fonts::FreeSansBold18pt7b);
  conferenceText("Invite details coming soon", 234, 227, &fonts::FreeSans9pt7b, DIM);
  d.drawRoundRect(174, 254, 120, 110, 14, 0x5AEB);
  conferenceText("QR", 234, 289, &fonts::FreeSansBold12pt7b, DIM, 110);
  conferenceText("reserved", 234, 324, &fonts::FreeSans9pt7b, DIM, 110);
  conferenceText("Placeholder / no destination yet", 234, 387, &fonts::Font0, DIM);
}
static void conferenceBadge() {
  auto &d = M5.Display;
  const uint8_t slot = navigation.network;
  const auto &profile = manualProfile.profile();
  const String &url = profile.urls[slot];
  if (conferenceExpanded && url.length()) {
    conferenceNetworkIcon(slot, 159, 77, 27);
    conferenceText(NETWORK_NAMES[slot], 251, 90, &fonts::FreeSans9pt7b, TFT_WHITE, 150);
    d.qrcode(url.c_str(), 99, 119, 270, 1, true);
    return;
  }
  drawInitWordmark(192, 63, 84, TFT_WHITE);
  conferenceAvatar(186, 89, 96);
  conferenceText(profile.name.length() ? profile.name : String("Your name"), 234, 202, &fonts::FreeSansBold12pt7b, TFT_WHITE, 314);
  conferenceNetworkIcon(slot, 154, 220, 24);
  conferenceText(NETWORK_NAMES[slot], 251, 232, &fonts::FreeSans9pt7b, TFT_WHITE, 155);
  if (url.length()) {
    d.qrcode(url.c_str(), 160, 249, 148, 1, true);
  } else {
    d.drawRoundRect(159, 255, 150, 119, 12, PANEL);
    conferenceText("No account yet", 234, 288, &fonts::FreeSans9pt7b, DIM, 200);
    conferenceText("Tap to configure", 234, 328, &fonts::FreeSans9pt7b, TFT_WHITE, 220);
    conferenceText("Swipe for other networks", 234, 388, &fonts::Font0, DIM);
  }
  for (int i = 0; i < 3; ++i) d.fillCircle(370, 286 + i*14, i == slot ? 3 : 2, i == slot ? TFT_WHITE : DIM);
}
static void conferenceHack() {
  drawInitWordmark(184, 70, 100, TFT_WHITE);
  conferenceText("Make it yours.", 234, 124, &fonts::FreeSansBold18pt7b);
  conferenceText("Build something for your badge", 234, 160, &fonts::FreeSans9pt7b, DIM);
  M5.Display.qrcode(HACK_URL, 135, 184, 198, 1, true);
  conferenceText("drop.workos.cloud/stopwatch", 234, 392, &fonts::Font0, DIM);
}
static void conferenceSettingsView() {
  auto &d = M5.Display;
  conferenceText("Settings", 234, 78, &fonts::FreeSansBold18pt7b);
  int battery = M5.Power.getBatteryLevel();
  conferenceText(battery < 0 ? String("Battery unavailable") : String("Battery ") + min(100, battery) + "%", 234, 108, &fonts::FreeSans9pt7b, DIM);
  conferenceText("Brightness", 234, 137, &fonts::FreeSans9pt7b, DIM);
  d.fillRoundRect(94, 146, 64, 42, 10, PANEL);
  d.fillRoundRect(310, 146, 64, 42, 10, PANEL);
  conferenceText("-", 126, 166, &fonts::FreeSansBold18pt7b, TFT_WHITE, 55, PANEL);
  conferenceText("+", 342, 166, &fonts::FreeSansBold18pt7b, TFT_WHITE, 55, PANEL);
  conferenceText(String(badgeSettings.brightness) + "%", 234, 168, &fonts::FreeSansBold12pt7b, TFT_WHITE, 120);
  conferenceText(conferenceClockValid() ? conferenceClockDateTimeText() : String("Date / time not set"), 234, 214, &fonts::FreeSans9pt7b, TFT_WHITE, 320);
  conferenceText("Sync time in phone setup", 234, 236, &fonts::Font0, DIM);
  d.fillRoundRect(78, 250, 152, 44, 10, PANEL);
  d.fillRoundRect(238, 250, 152, 44, 10, PANEL);
  conferenceText("Connect phone", 154, 272, &fonts::FreeSans9pt7b, TFT_WHITE, 144, PANEL);
  conferenceText("Touch test", 314, 272, &fonts::FreeSans9pt7b, TFT_WHITE, 144, PANEL);
  conferenceText("Orientation", 234, 328, &fonts::FreeSans9pt7b, DIM);
  for (int i = 0; i < 3; ++i) {
    int left = 91 + i*98;
    bool selected = int(badgeSettings.orientation) == i;
    uint16_t fill = selected ? 0x4228 : PANEL;
    d.fillRoundRect(left, 342, 90, 40, 9, fill);
    if (selected) d.drawRoundRect(left, 342, 90, 40, 9, ACCENT);
    if (i < 2) conferenceText(i == 0 ? "Free" : "Default", left + 45, 362, &fonts::FreeSans9pt7b, TFT_WHITE, 84, fill);
    else {
      // The pinned FreeSans font has no degree glyph; draw the degree mark.
      conferenceText("180", left + 40, 362, &fonts::FreeSans9pt7b, TFT_WHITE, 74, fill);
      d.drawCircle(left + 60, 355, 2, TFT_WHITE);
    }
  }
}
static void conferenceTouchTestView() {
  auto &d = M5.Display;
  conferenceText("Touch test", 234, 54, &fonts::FreeSansBold12pt7b);
  conferenceText("Touch targets / scale correction on", 234, 82, &fonts::Font0, DIM);
  const int points[][2] = {{234, 120}, {234, 234}, {234, 362}, {112, 234}, {356, 234}};
  for (const auto &point : points) {
    d.drawCircle(point[0], point[1], 14, TFT_WHITE);
    d.drawFastHLine(point[0] - 22, point[1], 45, TFT_WHITE);
    d.drawFastVLine(point[0], point[1] - 22, 45, TFT_WHITE);
  }
  conferenceText("White target / purple touch", 234, 175, &fonts::Font0, DIM);
  String pose = touchTest.rotation == 0 ? "Default" : touchTest.rotation == 2 ? "180 degrees" : "Sideways";
  conferenceText(pose + " / held during test", 234, 196, &fonts::Font0, DIM);
  if (touchTest.hasSample) {
    conferenceText(String(touchTest.pressed ? "Touch " : "Last touch ") + touchTest.x + ", " + touchTest.y,
      234, 282, &fonts::FreeSans9pt7b, ACCENT, 320);
    conferenceText(touchTest.sensorSample ? String("Sensor ") + touchTest.rawX + ", " + touchTest.rawY : String("Simulated input"),
      234, 306, &fonts::Font0, DIM);
    // Clip only the marker for drawing; retain actual coordinates in the readout.
    d.drawCircle(touchTest.x, touchTest.y, 9, ACCENT);
    d.fillCircle(touchTest.x, touchTest.y, 3, ACCENT);
  } else conferenceText("Your touch appears in purple", 234, 290, &fonts::Font0, DIM);
  conferenceText("Either button: back", 234, 409, &fonts::FreeSans9pt7b, TFT_WHITE, 280);
  conferenceText("Observation only / nothing saved", 234, 433, &fonts::Font0, DIM, 220);
}
static void conferenceSetupView() {
  auto &d = M5.Display;
  conferenceText(conferenceClockText(), 234, 35, &fonts::FreeSans9pt7b, DIM);
  conferenceText("Configure your badge", 234, 78, &fonts::FreeSansBold12pt7b);
  if (manualPortal.outcome() == ConferencePortalOutcome::Saved || manualPortal.outcome() == ConferencePortalOutcome::Cancelled) {
    conferenceText(manualPortal.outcome() == ConferencePortalOutcome::Saved ? "Saved" : "Cancelled", 234, 199, &fonts::FreeSansBold18pt7b);
    conferenceText("Closing setup", 234, 250, &fonts::FreeSans9pt7b, DIM);
  } else {
    conferenceText("Scan to join this badge's Wi-Fi", 234, 109, &fonts::FreeSans9pt7b, DIM);
    String qr = "WIFI:T:WPA;S:" + manualPortal.ssid() + ";P:" + manualPortal.password() + ";;";
    d.qrcode(qr.c_str(), 124, 132, 220, 1, true);
    conferenceText(manualPortal.ssid(), 234, 370, &fonts::Font0, DIM);
    conferenceText("Then open 192.168.4.1", 234, 390, &fonts::FreeSans9pt7b);
  }
  d.fillRoundRect(153, 406, 162, 35, 14, PANEL);
  conferenceText("Back / cancel", 234, 423, &fonts::FreeSans9pt7b);
}
static void conferenceRender() {
  auto &d = M5.Display;
  d.startWrite(); d.clearClipRect(); d.fillScreen(TFT_BLACK);
  if (touchTest.active) conferenceTouchTestView();
  else if (conferenceSetup) conferenceSetupView();
  else {
    switch (navigation.page) {
      case ConferencePage::Init: conferenceInit(); break;
      case ConferencePage::Schedule: conferenceSchedule(); break;
      case ConferencePage::AfterDark: conferenceAfterDark(); break;
      case ConferencePage::Badge: conferenceBadge(); break;
      case ConferencePage::Hack: conferenceHack(); break;
      case ConferencePage::Settings: conferenceSettingsView(); break;
    }
    conferenceChrome();
  }
  d.endWrite(); d.display(); conferenceRedraw = false;
  conferenceLastFrame = millis(); ++conferenceFrames;
}
static void conferenceCloseTouchTest() {
  touchTest.close(); navigation.cancel(); navigation.page = ConferencePage::Settings;
  conferenceSuppressTouchUntilRelease = true;
  conferenceOrientation.reset(conferenceOrientation.rotation());
  conferenceRedraw = true;
}
static void conferenceStartSetup(const char *testPassword = nullptr) {
  if (touchTest.active) return;
  navigation.cancel(); conferenceExpanded = false;
  conferenceSetupReturn = navigation.page == ConferencePage::Settings ? ConferencePage::Settings : ConferencePage::Badge;
  manualPortal.start(testPassword); conferenceSetup = manualPortal.active();
  if (!conferenceSetup) conferenceNotice("Setup unavailable");
  conferenceRedraw = true;
}
static void conferenceCloseSetup() {
  bool saved = manualPortal.outcome() == ConferencePortalOutcome::Saved;
  manualPortal.stop(); conferenceSetup = false; navigation.cancel();
  if (saved) navigation.page = conferenceSetupReturn;
  conferenceNotice(manualPortal.outcome() == ConferencePortalOutcome::Saved ? "Saved" : "Setup cancelled");
}
static void conferencePage(int direction) {
  if (touchTest.active) { conferenceCloseTouchTest(); return; }
  if (conferenceSetup) { conferenceCloseSetup(); return; }
  navigation.nextPage(direction); conferenceExpanded = false; conferenceToast = ""; conferenceRedraw = true;
}
static void conferenceButton(BadgeButtonAction action) {
  if (action == BadgeButtonAction::NONE) return;
  if (touchTest.active) { conferenceCloseTouchTest(); return; }
  if (conferenceSetup) { conferenceCloseSetup(); return; }
  if (action == BadgeButtonAction::SETTINGS) conferenceStartSetup();
  else conferencePage(conferencePusherDirection(action == BadgeButtonAction::BLUE, conferenceOrientation.rotation()));
}
static void conferenceTap(int x, int y) {
  if (conferenceSetup) { if (y >= 403 && x >= 130 && x <= 338) conferenceCloseSetup(); return; }
  if (y >= 180 && y <= 288) {
    if (x < 70) { conferencePage(-1); return; }
    if (x > 398) { conferencePage(1); return; }
  }
  if (navigation.page == ConferencePage::Settings) {
    auto action = conferenceSettingsHit(x, y);
    if (action == ConferenceSettingsAction::Dimmer || action == ConferenceSettingsAction::Brighter) {
      if (badgeSettings.adjustBrightness(action == ConferenceSettingsAction::Brighter ? 1 : -1, millis())) {
        M5.Display.setBrightness(badgeSettings.displayBrightness()); conferenceRedraw = true;
      }
    } else if (action == ConferenceSettingsAction::Connect) conferenceStartSetup();
    else if (action == ConferenceSettingsAction::TouchTest) {
      navigation.cancel(); conferenceToast = ""; conferenceExpanded = false;
      conferenceOrientation.reset(conferenceOrientation.rotation());
      touchTest.open(conferenceOrientation.rotation()); conferenceRedraw = true;
    }
    else if (action >= ConferenceSettingsAction::Free) {
      auto mode = ConferenceOrientationMode(int(action) - int(ConferenceSettingsAction::Free));
      if (badgeSettings.setOrientation(mode, millis())) { conferenceOrientationPending = true; conferenceRedraw = true; }
    }
    return;
  }
  if (navigation.page == ConferencePage::Badge && x >= 80 && x <= 388 && y >= 70 && y <= 399) {
    if (!manualProfile.profile().urls[navigation.network].length()) conferenceStartSetup();
    else { conferenceExpanded = !conferenceExpanded; conferenceRedraw = true; }
  }
}
static void conferenceGestureEnd(int x, int y, uint32_t now, bool clicked) {
  auto result = navigation.end(x, y, now, clicked, !conferenceSetup);
  if (result == ConferenceGesture::Tap) conferenceTap(x, y);
  else if (result != ConferenceGesture::None) {
    conferenceExpanded = false; conferenceRedraw = true;
    if (result == ConferenceGesture::Network) conferenceRememberNetwork();
  }
}
static void conferenceStatus(const char *nonce = nullptr) {
  uint8_t mask = 0;
  for (int i = 0; i < 3; ++i) if (manualProfile.profile().urls[i].length()) mask |= 1 << i;
  Serial.printf("CONFERENCE_STATUS {\"build\":\"%s\",\"page_count\":6,\"brightness_percent\":%u,\"orientation_mode\":\"%s\",\"preferences_pending\":%s,\"preference_writes\":%u,\"schedule_current\":%d,\"page\":%u,\"network\":%u,\"configured_mask\":%u,\"scroll\":%d,\"scroll_max\":%d,\"expanded\":%s,\"setup\":%s,\"wifi_mode\":%u,\"ap_clients\":%u,\"bluetooth\":%u,\"rotation\":%u,\"avatar\":%s,\"store_ready\":%s,\"clock_valid\":%s,\"rtc\":%s,\"frames\":%u,\"inputs\":%u,\"max_loop_gap_ms\":%u,\"flash_bytes\":%u,\"psram_bytes\":%u,\"board\":%u,\"name_present\":%s,\"nonce\":\"%s\"}\n",
    CONFERENCE_BUILD, unsigned(badgeSettings.brightness), badgeSettings.orientationName(), badgeSettings.pending() ? "true" : "false", unsigned(conferencePreferenceWrites), conferenceCurrentItem, unsigned(navigation.page), unsigned(navigation.network), unsigned(mask), navigation.scroll, ConferenceNavigation::MAX_SCROLL,
    conferenceExpanded ? "true" : "false", conferenceSetup ? "true" : "false", unsigned(WiFi.getMode()), unsigned(WiFi.softAPgetStationNum()), unsigned(esp_bt_controller_get_status()),
    unsigned(conferenceOrientation.rotation()), manualProfile.avatarPixels() ? "true" : "false", manualProfile.ready() ? "true" : "false",
    conferenceClockValid() ? "true" : "false", M5.Rtc.isEnabled() ? "true" : "false", unsigned(conferenceFrames), unsigned(conferenceInputCount), unsigned(conferenceMaxGap), unsigned(ESP.getFlashChipSize()), unsigned(ESP.getPsramSize()), unsigned(M5.getBoard()), manualProfile.profile().name.length() ? "true" : "false", conference_clock::validNonce(nonce) ? nonce : "");
}
static void conferenceTouchTestStatus(const char *nonce) {
  JsonDocument status;
  status["active"] = touchTest.active; status["pressed"] = touchTest.pressed;
  status["sample"] = touchTest.hasSample; status["sensor"] = touchTest.sensorSample;
  status["x"] = touchTest.x; status["y"] = touchTest.y;
  status["raw_x"] = touchTest.rawX; status["raw_y"] = touchTest.rawY;
  status["rotation"] = touchTest.rotation;
  status["scale_trial"] = true;
  status["nonce"] = conference_clock::validNonce(nonce) ? nonce : "";
  Serial.print("TOUCH_TEST_STATUS "); serializeJson(status, Serial); Serial.println();
}
static void conferenceCapture() {
  // Setup includes ephemeral Wi-Fi credentials; never export that screen.
  if (conferenceSetup) { Serial.println("CAPTURE_REJECTED"); return; }
  conferenceRender();
  const int w = M5.Display.width(), h = M5.Display.height();
  uint8_t row[468 * 3]; if (w != 468 || h != 468) { Serial.println("CAPTURE_REJECTED"); return; }
  Serial.setTxTimeoutMs(10); const uint32_t until = millis() + 5000; bool complete = true;
  Serial.printf("BADGE_CAPTURE %d %d\n", w, h);
  for (int y = 0; y < h; ++y) {
    if (conferenceDue(until)) { complete = false; break; }
    M5.Display.readRectRGB(0, y, w, 1, row);
    if (Serial.write(row, sizeof(row)) != sizeof(row)) { complete = false; break; }
  }
  Serial.println(complete ? "\nBADGE_CAPTURE_END" : "\nBADGE_CAPTURE_ABORTED"); Serial.setTxTimeoutMs(0);
  conferenceLastLoop = millis();
}
static void conferenceCommands() {
  while (Serial.available()) {
    char ch = char(Serial.read());
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (conferenceSerial.length() < 1024 && !conferenceSerialOverflow) conferenceSerial += ch;
      else conferenceSerialOverflow = true;
      continue;
    }
    if (conferenceSerialOverflow) { conferenceSerial = ""; conferenceSerialOverflow = false; Serial.println("COMMAND_REJECTED"); continue; }
    JsonDocument cmd; auto error = deserializeJson(cmd, conferenceSerial); conferenceSerial = "";
    if (error) { Serial.println("COMMAND_REJECTED"); continue; }
    if (conferenceClockCommand(cmd)) { conferenceRedraw = true; continue; }
    String op = cmd["op"] | "";
    if (op == "status") { if (cmd["reset_metrics"] | false) conferenceMaxGap = 0; conferenceStatus(cmd["nonce"] | static_cast<const char *>(nullptr)); }
    else if (op == "touch_test_status") conferenceTouchTestStatus(cmd["nonce"] | static_cast<const char *>(nullptr));
    else if (op == "portal_status") {
      const auto &d = manualPortal.transportDiagnostics();
      JsonDocument status;
      status["accepts"] = d.accepts; status["closes"] = d.closes;
      status["reason"] = d.lastClose; status["errno"] = d.lastErrno;
      status["age_ms"] = d.lastAgeMs; status["method"] = d.lastMethod;
      status["phase"] = d.lastPhase; status["http_status"] = d.lastStatus;
      status["header_bytes"] = d.lastHeaderBytes;
      status["body_bytes"] = d.lastBodyBytes; status["expected_body"] = d.lastExpectedBody;
      status["active"] = d.active; status["current_method"] = d.currentMethod;
      status["current_phase"] = d.currentPhase; status["current_age_ms"] = d.currentAgeMs;
      status["current_body_bytes"] = d.currentBodyBytes; status["current_expected_body"] = d.currentExpectedBody;
      status["post_attempts"] = d.postAttempts; status["post_closes"] = d.postCloses;
      status["last_post_reason"] = d.lastPostClose; status["last_post_errno"] = d.lastPostErrno;
      status["last_post_age_ms"] = d.lastPostAgeMs; status["last_post_body_bytes"] = d.lastPostBodyBytes;
      Serial.print("PORTAL_STATUS "); serializeJson(status, Serial); Serial.println();
    }
    else if (op == "button") {
      String value = cmd["value"] | "";
      if (value != "blue" && value != "yellow" && value != "both") { Serial.println("COMMAND_REJECTED"); continue; }
      conferenceButton(value == "both" ? BadgeButtonAction::SETTINGS : value == "blue" ? BadgeButtonAction::BLUE : BadgeButtonAction::YELLOW);
      conferenceRender(); conferenceStatus();
    } else if (op == "page" && cmd["step"].is<int>() && abs(cmd["step"].as<int>()) == 1) {
      conferencePage(cmd["step"].as<int>()); conferenceRender(); conferenceStatus();
    } else if (op == "touch") {
      int x = cmd["x"] | -1, y = cmd["y"] | -1; String phase = cmd["phase"] | "";
      if (x < 0 || x >= 468 || y < 0 || y >= 468 || (phase != "begin" && phase != "move" && phase != "end")) { Serial.println("COMMAND_REJECTED"); continue; }
      if (touchTest.active) {
        if (touchTest.observe(x, y, phase != "end")) conferenceRedraw = true;
      } else {
        if (phase == "begin") navigation.begin(x, y, millis());
        if (phase == "move" && navigation.move(x, y, !conferenceSetup) != ConferenceGesture::None) conferenceRedraw = true;
        if (phase == "end") conferenceGestureEnd(x, y, millis(), true);
      }
      if (conferenceRedraw) conferenceRender(); conferenceStatus();
    } else if (op == "setup_test") {
      // Input-only ephemeral AP password allows local verification without any credential export.
      String password = cmd["password"] | "";
      bool valid = password.length() >= 12 && password.length() <= 32;
      for (char c : password) if (!isalnum(static_cast<unsigned char>(c))) valid = false;
      if (!valid || conferenceSetup || touchTest.active) { Serial.println("COMMAND_REJECTED"); continue; }
      conferenceStartSetup(password.c_str()); password = "";
      Serial.printf("CONFERENCE_SETUP {\"active\":%s,\"ssid\":\"%s\"}\n", conferenceSetup ? "true" : "false", manualPortal.ssid().c_str());
    } else if (op == "initialize_conference_storage") {
      const char *nonce = cmd["nonce"] | static_cast<const char *>(nullptr);
      bool confirmed = (cmd["confirm"] | String("")) == "ERASE_FFAT_FOR_CONFERENCE";
      bool success = false;
      if (confirmed && conference_clock::validNonce(nonce) && !conferenceSetup) success = manualProfile.initializeForConference();
      Serial.printf("STORAGE_ACK {\"nonce\":\"%s\",\"ok\":%s,\"store_ready\":%s}\n", conference_clock::validNonce(nonce) ? nonce : "", success ? "true" : "false", manualProfile.ready() ? "true" : "false");
    } else if (op == "clear_manual_profile" && cmd["confirm"].is<bool>() && cmd["confirm"].as<bool>()) {
      if (conferenceSetup) conferenceCloseSetup();
      bool success = manualProfile.clear(); conferenceExpanded = false; conferenceRedraw = true;
      Serial.printf("CONFERENCE_CLEAR {\"success\":%s}\n", success ? "true" : "false");
    } else if (op == "capture_badge" || op == "capture") conferenceCapture();
    else if (op == "reboot") { Serial.println("CONFERENCE_REBOOT"); Serial.flush(); ESP.restart(); }
    else Serial.println("COMMAND_REJECTED");
  }
}
void setup() {
  Serial.setTxBufferSize(4096); Serial.begin(115200); Serial.setTxTimeoutMs(0); conferenceSerial.reserve(1024);
  auto cfg = M5.config(); cfg.internal_imu = true; cfg.internal_rtc = true;
  cfg.internal_mic = false; cfg.internal_spk = false; cfg.fallback_board = m5::board_t::board_M5StopWatch;
  M5.begin(cfg);
  conferencePreferencesReady = conferenceUi.begin("conference_ui", false);
  badgeSettings.restore(conferenceUi.getUInt("prefs", 0));
  conferenceOrientation.reset(badgeSettings.automatic() ? 2 : badgeSettings.fixedRotation());
  M5.Display.setRotation(conferenceOrientation.rotation()); M5.Display.setBrightness(badgeSettings.displayBrightness());
  // No station credentials loaded, no cloud clients initialized, no audio workers.
  WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_OFF);
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) esp_bt_controller_disable();
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) esp_bt_controller_deinit();
  conferenceClockBegin(); manualProfile.begin(); conferenceProfileRevision = manualProfile.revision();
  navigation.network = conferenceUi.getUChar("network", 0);
  if (navigation.network >= 3) navigation.network = 0;
  conferenceLastClock = conferenceClockDateTimeText(); conferenceRender();
  Serial.println("CONFERENCE_READY"); conferenceStatus();
}
void loop() {
  uint32_t now = millis();
  if (conferenceLastLoop) conferenceMaxGap = max(conferenceMaxGap, now - conferenceLastLoop);
  conferenceLastLoop = now; M5.update(); conferenceCommands();
  bool activeBefore = manualPortal.active(); manualPortal.tick();
  if (manualProfile.revision() != conferenceProfileRevision) {
    conferenceProfileRevision = manualProfile.revision(); conferenceRedraw = true;
  }
  if (conferenceSetup && activeBefore && !manualPortal.active()) {
    conferenceSetup = false; navigation.cancel(); conferenceExpanded = false;
    if (manualPortal.outcome() == ConferencePortalOutcome::Saved) { navigation.page = conferenceSetupReturn; conferenceNotice("Saved"); }
    else conferenceNotice(manualPortal.outcome() == ConferencePortalOutcome::TimedOut ? "Setup timed out" : "Setup cancelled");
  }
  auto button = conferenceButtons.update(M5.BtnA.isPressed(), M5.BtnB.isPressed(), millis());
  if (button != BadgeButtonAction::NONE) { ++conferenceInputCount; navigation.cancel(); conferenceButton(button); }
  if (conferenceSuppressTouchUntilRelease && M5.Touch.getCount() == 0) conferenceSuppressTouchUntilRelease = false;
  if (M5.Touch.getCount() && !conferenceSuppressTouchUntilRelease) {
    const auto &touch = M5.Touch.getDetail();
    if (touchTest.active) {
      if (touch.isPressed()) {
        // Detail coordinates latch until the driver's flick threshold. Read the
        // same raw sample and transform once to show even small finger movement.
        auto raw = M5.Touch.getTouchPointRaw(); auto point = raw;
        M5.Display.convertRawXY(&point, 1);
        auto scaled = conferenceScaleTouch(point.x, point.y, conferenceOrientation.rotation());
        if (touchTest.observe(scaled.x, scaled.y, true, true, raw.x, raw.y)) conferenceRedraw = true;
      }
      if (touch.wasReleased()) { ++conferenceInputCount; if (touchTest.release()) conferenceRedraw = true; }
    } else {
      auto scaled = conferenceScaleTouch(touch.x, touch.y, conferenceOrientation.rotation());
      if (touch.wasPressed()) navigation.begin(scaled.x, scaled.y, millis());
      if (touch.isPressed() && navigation.move(scaled.x, scaled.y, !conferenceSetup) != ConferenceGesture::None) conferenceRedraw = true;
      if (touch.wasReleased()) { ++conferenceInputCount; conferenceGestureEnd(scaled.x, scaled.y, millis(), touch.wasClicked()); }
    }
  }
  if (!touchTest.active && conferenceOrientationPending && !navigation.touching() && M5.Touch.getCount() == 0) {
    uint8_t rotation = badgeSettings.automatic() ? conferenceOrientation.rotation() : badgeSettings.fixedRotation();
    conferenceOrientation.reset(rotation); M5.Display.setRotation(rotation);
    conferenceOrientationPending = false; conferenceRedraw = true;
  }
  if (!touchTest.active && badgeSettings.automatic() && !conferenceOrientationPending && M5.Imu.isEnabled() && uint32_t(millis() - conferenceLastImu) >= 50) {
    conferenceLastImu = millis();
    if (M5.Imu.update() & m5::IMU_Class::sensor_mask_accel) {
      auto data = M5.Imu.getImuData();
      if (conferenceOrientation.update(data.accel.y, data.accel.x, data.accel.z, millis(), M5.Touch.getCount() != 0 || navigation.touching())) {
        M5.Display.setRotation(conferenceOrientation.rotation()); navigation.cancel(); conferenceRedraw = true;
      }
    } else conferenceOrientation.invalidate();
  }
  int currentItem = conferenceCurrentScheduleItem(CONFERENCE_AGENDA, CONFERENCE_AGENDA_COUNT, conferenceClockEpoch(), conferenceClockValid());
  if (currentItem != conferenceCurrentItem) { conferenceCurrentItem = currentItem; if (navigation.page == ConferencePage::Schedule) conferenceRedraw = true; }
  String clock = conferenceClockDateTimeText();
  if (clock != conferenceLastClock) { conferenceLastClock = clock; conferenceRedraw = true; }
  if (conferenceToast.length() && conferenceDue(conferenceToastUntil)) { conferenceToast = ""; conferenceRedraw = true; }
  if (conferenceSaveSelectionAt && conferenceDue(conferenceSaveSelectionAt)) {
    conferenceSaveSelectionAt = 0;
    if (conferenceUi.getUChar("network", 255) != navigation.network) conferenceUi.putUChar("network", navigation.network);
  }
  if (badgeSettings.saveDue(millis())) {
    if (conferencePreferencesReady && conferenceUi.putUInt("prefs", badgeSettings.encoded()) == sizeof(uint32_t)) {
      ++conferencePreferenceWrites; badgeSettings.saved(); conferenceRedraw = true;
    } else { badgeSettings.saveFailed(millis()); conferenceNotice("Settings not saved"); }
  }
  if (!conferenceSetup && navigation.page == ConferencePage::Settings && uint32_t(millis() - conferenceLastPowerRead) >= 10000) {
    conferenceLastPowerRead = millis(); conferenceRedraw = true;
  }
  if (!conferenceSetup && navigation.page == ConferencePage::Init && uint32_t(millis() - conferenceLastFrame) >= 125) conferenceRedraw = true;
  if (conferenceRedraw && (!touchTest.active || uint32_t(millis() - conferenceLastFrame) >= 33)) conferenceRender();
  delay(8);
}
