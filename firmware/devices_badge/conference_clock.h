#pragma once

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <sys/time.h>
#include <time.h>

// The RX8130 calendar contains UTC. NVS contains only the display UTC offset;
// an old flash timestamp is never used to infer elapsed time while unpowered.
namespace conference_clock {
constexpr int64_t MIN_EPOCH = 1704067200LL;  // 2024-01-01 UTC
constexpr int64_t MAX_EPOCH = 4102444800LL;  // 2100-01-01, exclusive
constexpr uint8_t FLAGS = 0x1d;
constexpr uint8_t VLF = 0x02;  // Oscillator-stop/data-invalid flag, not VBLF.
static bool valid = false;
static int offsetMinutes = 0;
static const char* source = "unset";

inline bool validEpoch(int64_t epoch) { return epoch >= MIN_EPOCH && epoch < MAX_EPOCH; }
inline bool validOffset(int offset) { return offset >= -840 && offset <= 840; }
inline bool validNonce(const char* nonce) {
  if (!nonce) return false;
  size_t length = strlen(nonce);
  if (length < 8 || length > 64) return false;
  for (size_t i = 0; i < length; ++i) {
    char c = nonce[i];
    if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f')) return false;
  }
  return true;
}

inline int bcd(uint8_t value) {
  return (value & 15) <= 9 && (value >> 4) <= 9 ? (value >> 4) * 10 + (value & 15) : -1;
}
inline int64_t calendarEpoch(int year, int month, int day, int hour, int minute, int second) {
  static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (year < 2024 || year > 2099 || month < 1 || month > 12 || hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 || second < 0 || second > 59) return -1;
  int maxDay = lengths[month - 1] + (month == 2 && year % 4 == 0);
  if (day < 1 || day > maxDay) return -1;
  int64_t days = 0;
  for (int y = 1970; y < year; ++y) days += 365 + (y % 4 == 0);
  for (int m = 1; m < month; ++m) days += lengths[m - 1] + (m == 2 && year % 4 == 0);
  days += day - 1;
  return days * 86400 + hour * 3600 + minute * 60 + second;
}

inline bool readRtc(int64_t& epoch) {
  auto* rtc = M5.Rtc.getRtcInstancePtr();
  uint8_t flags = 0, control = 0, bytes[7] = {};
  if (!rtc || rtc->getAddress() != 0x32 || !rtc->readRegister(FLAGS, &flags, 1) ||
      (flags & VLF) || !rtc->readRegister(0x1e, &control, 1) || (control & 0x40) ||
      !rtc->readRegister(0x10, bytes, sizeof(bytes))) return false;
  // Read one contiguous calendar transaction; reject bad BCD and impossible dates.
  if ((bytes[0] & 0x80) || (bytes[1] & 0x80) || (bytes[2] & 0xc0) ||
      (bytes[4] & 0xc0) || (bytes[5] & 0xe0)) return false;
  int yy = bcd(bytes[6]);
  if (yy < 0) return false;
  epoch = calendarEpoch(2000 + yy, bcd(bytes[5]), bcd(bytes[4]), bcd(bytes[2]),
                        bcd(bytes[1]), bcd(bytes[0]));
  return validEpoch(epoch);
}

inline bool setSystem(int64_t epoch) {
  timeval value = {};
  value.tv_sec = static_cast<time_t>(epoch);
  return settimeofday(&value, nullptr) == 0;
}

inline const char* provision(int64_t epoch, int offset) {
  if (!validEpoch(epoch) || !validOffset(offset)) return "invalid_value";
  auto* rtc = M5.Rtc.getRtcInstancePtr();
  if (!rtc || rtc->getAddress() != 0x32) return "rtc_unavailable";
  uint8_t flags;
  if (!rtc->readRegister(FLAGS, &flags, 1)) return "rtc_read_failed";
  valid = false;
  source = "unset";
  // Initialize unused timing/interrupt registers after detected power loss. The
  // board-specific M5Unified begin() owns the power-switch/charge configuration.
  if (flags & VLF) {
    const uint8_t unused[] = {0x80, 0x80, 0x80, 0, 0, 0xc0};
    if (!rtc->writeRegister(0x17, unused, sizeof(unused)) ||
        !rtc->writeRegister8(0x30, 0) || !rtc->writeRegister8(0x1e, 0)) return "rtc_init_failed";
  }
  time_t seconds = static_cast<time_t>(epoch);
  tm utc = {};
  if (!gmtime_r(&seconds, &utc)) return "invalid_value";
  m5::rtc_datetime_t target(utc);
  // RTC_Base exposes the write result that RTC_Class::setDateTime discards.
  if (!rtc->setDateTime(&target.date, &target.time)) return "rtc_write_failed";
  if (!rtc->writeRegister8(FLAGS, flags & ~VLF)) return "rtc_flag_failed";
  int64_t readback;
  if (!readRtc(readback) || readback < epoch || readback > epoch + 1) return "rtc_verify_failed";
  Preferences prefs;
  if (!prefs.begin("conf-clock", false)) return "offset_store_failed";
  bool saved = prefs.isKey("offset") && prefs.getInt("offset", 9999) == offset;
  if (!saved) saved = prefs.putInt("offset", offset) == sizeof(int32_t);
  prefs.end();
  if (!saved) return "offset_store_failed";
  if (!setSystem(readback)) return "system_clock_failed";
  offsetMinutes = offset;
  valid = true;
  source = "computer";
  return nullptr;
}
}  // namespace conference_clock

inline void conferenceClockBegin() {
  using namespace conference_clock;
  valid = false;
  source = "unset";
  offsetMinutes = 0;
  Preferences prefs;
  if (prefs.begin("conf-clock", true)) {
    int stored = prefs.getInt("offset", 0);
    if (validOffset(stored)) offsetMinutes = stored;
    prefs.end();
  }
  int64_t epoch;
  if (readRtc(epoch) && setSystem(epoch)) { valid = true; source = "rtc"; }
}

inline bool conferenceClockValid() { return conference_clock::valid; }
inline const char* conferenceClockSource() { return conference_clock::source; }
inline String conferenceClockText() {
  if (!conference_clock::valid) return "--:--";
  time_t local = time(nullptr) + conference_clock::offsetMinutes * 60;
  tm result = {};
  if (!gmtime_r(&local, &result)) return "--:--";
  char text[6];
  snprintf(text, sizeof(text), "%02d:%02d", result.tm_hour, result.tm_min);
  return String(text);
}

// Returns true when the caller should stop dispatching this serial command.
// Only time/status metadata is returned; no profile or credential diagnostics.
inline bool conferenceClockCommand(JsonDocument& cmd) {
  using namespace conference_clock;
  const char* op = cmd["op"] | "";
  bool setting = strcmp(op, "clock_set") == 0;
  if (!setting && strcmp(op, "clock_status") != 0) return false;
  const char* nonce = cmd["nonce"].is<const char*>() ? cmd["nonce"].as<const char*>() : nullptr;
  const char* error = nullptr;
  if (!validNonce(nonce)) error = "invalid_nonce";
  else if (setting) {
    if (!cmd["epoch"].is<int64_t>() || !cmd["offset_minutes"].is<int>()) error = "invalid_value";
    else error = provision(cmd["epoch"].as<int64_t>(), cmd["offset_minutes"].as<int>());
  }
  int64_t rtcEpoch = 0;
  bool rtcValid = readRtc(rtcEpoch);
  if (!rtcValid) { valid = false; source = "unset"; }
  JsonDocument reply;
  reply["protocol"] = 1;
  if (validNonce(nonce)) reply["nonce"] = nonce;
  reply["ok"] = error == nullptr && valid && rtcValid;
  reply["valid"] = valid && rtcValid;
  reply["source"] = source;
  reply["epoch"] = valid ? int64_t(time(nullptr)) : 0;
  reply["rtc_epoch"] = rtcValid ? rtcEpoch : 0;
  reply["offset_minutes"] = offsetMinutes;
  if (error) reply["error"] = error;
  else if (!valid || !rtcValid) reply["error"] = "clock_unset";
  Serial.print(setting ? "CLOCK_ACK " : "CLOCK_STATUS ");
  serializeJson(reply, Serial);
  Serial.println();
  return true;
}
