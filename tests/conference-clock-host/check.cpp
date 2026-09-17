#include <cassert>
#include <iostream>
#include <sys/time.h>
#include <time.h>
#include "M5Unified.h"

static bool systemSetFails = false;
static int testSettimeofday(const timeval* value, const void*) {
  if (systemSetFails) return -1;
  testNow = value->tv_sec;
  return 0;
}
static time_t testTime(time_t* value) { if (value) *value=testNow;return testNow; }
#define settimeofday testSettimeofday
#define time(...) testTime(__VA_ARGS__)
#include "../../firmware/devices_badge/conference_clock.h"
#undef settimeofday
#undef time

static void reset() {
  M5 = M5Fake{};
  Preferences::available = Preferences::writable = true;
  Preferences::stored = false;
  Preferences::value = 0;
  testNow = 1789560000;
  systemSetFails=false;
  Serial.output.clear();
}
static JsonDocument command(const char* json) {
  JsonDocument input;
  assert(!deserializeJson(input,json));
  assert(conferenceClockCommand(input));
  auto pos=Serial.output.find(' ');
  JsonDocument output;
  assert(!deserializeJson(output,Serial.output.substr(pos+1)));
  Serial.output.clear();
  return output;
}
int main() {
  using namespace conference_clock;
  reset();
  assert(calendarEpoch(2024,2,29,0,0,0)==1709164800);
  assert(calendarEpoch(2025,2,29,0,0,0)==-1);
  assert(calendarEpoch(2099,12,31,23,59,59)==MAX_EPOCH-1);
  assert(calendarEpoch(2026,4,31,0,0,0)==-1);
  assert(calendarEpoch(2026,1,1,24,0,0)==-1);
  assert(!validEpoch(MIN_EPOCH-1) && !validEpoch(MAX_EPOCH));
  assert(validOffset(-840) && validOffset(840) && !validOffset(841));
  assert(validNonce("abcdef0123456789") && !validNonce("password") && !validNonce("abc"));
  conferenceClockBegin();
  assert(conferenceClockValid() && std::string(conferenceClockSource())=="rtc");
  M5.Rtc.rtc.memory[FLAGS]=VLF;
  conferenceClockBegin();
  assert(!conferenceClockValid() && conferenceClockText()=="--:--");
  auto result=command(R"({"op":"clock_set","epoch":1789560000,"offset_minutes":-420,"nonce":"0123456789abcdef"})");
  assert(result["ok"].as<bool>() && result["rtc_epoch"].as<int64_t>()==1789560000);
  assert(conferenceClockText()=="05:00");
  assert(Preferences::value==-420 && (M5.Rtc.rtc.memory[FLAGS]&VLF)==0);
  testNow+=60;
  assert(conferenceClockText()=="05:01");
  conferenceClockBegin();
  assert(conferenceClockValid() && offsetMinutes==-420);
  const char* invalid[]={
    R"({"op":"clock_set","epoch":"1789560000","offset_minutes":0,"nonce":"0123456789abcdef"})",
    R"({"op":"clock_set","epoch":1789560000,"offset_minutes":true,"nonce":"0123456789abcdef"})",
    R"({"op":"clock_set","epoch":1789560000,"offset_minutes":900,"nonce":"0123456789abcdef"})",
    R"({"op":"clock_set","epoch":0,"offset_minutes":0,"nonce":"0123456789abcdef"})",
    R"({"op":"clock_set","epoch":4102444800,"offset_minutes":0,"nonce":"0123456789abcdef"})",
    R"({"op":"clock_set","epoch":1789560000,"offset_minutes":0,"nonce":"PRIVATE"})"};
  for(const char* input:invalid) { result=command(input);assert(!result["ok"].as<bool>()); }
  M5.Rtc.rtc.memory[FLAGS]=VLF;
  result=command(R"({"op":"clock_status","nonce":"0123456789abcdef"})");
  assert(!result["ok"].as<bool>() && !conferenceClockValid());
  reset();M5.Rtc.rtc.readable=false;conferenceClockBegin();assert(!conferenceClockValid());
  reset();M5.Rtc.enabled=false;assert(std::string(provision(1789560000,0))=="rtc_unavailable");
  reset();M5.Rtc.rtc.writable=false;assert(std::string(provision(1789560000,0))=="rtc_write_failed");
  reset();Preferences::writable=false;assert(std::string(provision(1789560000,0))=="offset_store_failed");
  reset();systemSetFails=true;assert(std::string(provision(1789560000,0))=="system_clock_failed");
  reset();M5.Rtc.rtc.raw=true;
  M5.Rtc.rtc.memory[0x10]=0x99;
  int64_t epoch=0;assert(!readRtc(epoch));
  reset();M5.Rtc.rtc.memory[0x1e]=0x40;assert(!readRtc(epoch));
  std::cout<<"conference clock validation, RTC loss, readback, offset persistence, and serial contract passed\n";
}
