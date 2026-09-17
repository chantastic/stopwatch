#include <cassert>
#include <iostream>
#include <sys/time.h>
#include <time.h>
#include "M5Unified.h"

static bool systemSetFails = false;
static bool systemSetIgnored = false;
static int testSettimeofday(const timeval* value, const void*) {
  if (systemSetFails) return -1;
  if (systemSetIgnored) return 0;
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
  Preferences::writes = 0;
  testNow = 1789560000;
  testRtcTicks = 0;
  systemSetFails=false;
  systemSetIgnored=false;
  conference_clock::valid=false;
  conference_clock::offsetMinutes=0;
  conference_clock::source="unset";
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
static JsonDocument phone(const char* json, bool expectedSuccess) {
  JsonDocument input, output;
  assert(!deserializeJson(input,json));
  output["old_private_field"]="must disappear";
  assert(conferenceClockSyncPhone(input,output)==expectedSuccess);
  assert(output["ok"].is<bool>() && output["ok"].as<bool>()==expectedSuccess);
  assert(output["old_private_field"].isNull());
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
  testRtcTicks+=60;
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

  // Phone clock adoption is independent of profiles and preserves UTC storage.
  reset();
  assert(conferenceClockEpoch()==0 && conferenceClockDateText()=="---- -- --");
  auto synced=phone(R"({"epoch":1704069000,"offset_minutes":-60,"timezone":"Europe/London"})",true);
  assert(synced["source"].as<std::string>()=="phone");
  assert(synced["epoch"].as<int64_t>()==1704069000);
  assert(synced["rtc_epoch"].as<int64_t>()==1704069000);
  assert(synced["timezone"].isNull());
  assert(conferenceClockDateTimeText()=="2023-12-31 23:30");
  assert(conferenceClockDateText()=="2023-12-31" && conferenceClockText()=="23:30");
  assert(conferenceClockOffsetMinutes()==-60 && conferenceClockEpoch()==1704069000);
  assert(Preferences::writes==1);
  phone(R"({"epoch":1704069001,"offset_minutes":-60})",true);
  assert(Preferences::writes==1);  // Retrying or reopening setup does not rewrite identical offset.
  conferenceClockBegin();
  assert(std::string(conferenceClockSource())=="rtc" && conferenceClockOffsetMinutes()==-60);
  assert(conferenceClockDateTimeText()=="2023-12-31 23:30" && Preferences::writes==1);
  phone(R"({"epoch":1704151800,"offset_minutes":60})",true);
  assert(conferenceClockDateTimeText()=="2024-01-02 00:30");
  assert(Preferences::writes==2 && conferenceClockOffsetMinutes()==60);
  tm local={};assert(conferenceClockLocalTime(local)&&local.tm_mday==2&&local.tm_hour==0);

  const char* badPhone[]={
    R"({"epoch":"1704069000","offset_minutes":0})",
    R"({"epoch":1704069000.5,"offset_minutes":0})",
    R"({"epoch":1704069000.0,"offset_minutes":0})",
    R"({"epoch":true,"offset_minutes":0})",
    R"({"epoch":1704069000,"offset_minutes":false})",
    R"({"epoch":1704069000,"offset_minutes":1.5})",
    R"({"epoch":1704069000,"offset_minutes":"60"})",
    R"({"epoch":1704067199,"offset_minutes":0})",
    R"({"epoch":4102444800,"offset_minutes":0})",
    R"({"epoch":1704069000,"offset_minutes":-841})",
    R"({"epoch":1704069000,"offset_minutes":841})",
    R"({"offset_minutes":0})",R"({"epoch":1704069000})"};
  for(const char* input:badPhone) {
    auto rejected=phone(input,false);
    assert(rejected["error"].as<std::string>()=="invalid_value");
    assert(conferenceClockEpoch()==1704151800 && conferenceClockOffsetMinutes()==60);
    assert(Preferences::writes==2);
  }
  reset();M5.Rtc.rtc.writable=false;
  assert(phone(R"({"epoch":1704069000,"offset_minutes":0})",false)["error"].as<std::string>()=="rtc_write_failed");
  assert(!conferenceClockValid()&&Preferences::writes==0);
  reset();M5.Rtc.rtc.readable=false;
  assert(phone(R"({"epoch":1704069000,"offset_minutes":0})",false)["error"].as<std::string>()=="rtc_read_failed");
  assert(!conferenceClockValid()&&Preferences::writes==0);
  reset();M5.Rtc.rtc.ignoreCalendarWrite=true;
  assert(phone(R"({"epoch":1704069000,"offset_minutes":0})",false)["error"].as<std::string>()=="rtc_verify_failed");
  assert(!conferenceClockValid()&&Preferences::writes==0);
  reset();systemSetIgnored=true;
  assert(phone(R"({"epoch":1704069000,"offset_minutes":0})",false)["error"].as<std::string>()=="system_clock_failed");
  assert(!conferenceClockValid());
  reset();Preferences::writable=false;
  assert(phone(R"({"epoch":1704069000,"offset_minutes":0})",false)["error"].as<std::string>()=="offset_store_failed");
  assert(!conferenceClockValid());
  reset();M5.Rtc.rtc.memory[FLAGS]=VLF;
  phone(R"({"epoch":1704069000,"offset_minutes":840})",true);
  assert(conferenceClockValid()&&conferenceClockText()=="14:30");
  testNow=0;
  assert(!conferenceClockValid()&&conferenceClockEpoch()==0&&conferenceClockDateTimeText()=="---- -- -- --:--");
  std::cout<<"conference clock validation, RTC loss/readback, phone adoption/date rollover, offset write avoidance, and USB contract passed\n";
}
