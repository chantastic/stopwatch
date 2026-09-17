#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <ctime>
#include <algorithm>

#ifndef CONFERENCE_CLOCK_TEST_EXTERNAL_STRING
using String = std::string;
#endif
inline int64_t testNow = 1789560000;
inline int64_t testRtcTicks = 0;
struct SerialFake {
  std::string output;
  void print(const char* value) { output += value; }
  void println() { output += '\n'; }
  size_t write(uint8_t value) { output += char(value); return 1; }
  size_t write(const uint8_t* value, size_t count) { output.append(reinterpret_cast<const char*>(value), count); return count; }
};
inline SerialFake Serial;

namespace m5 {
struct rtc_date_t { int year, month, date, weekDay; };
struct rtc_time_t { int hours, minutes, seconds; };
struct rtc_datetime_t {
  rtc_date_t date;
  rtc_time_t time;
  explicit rtc_datetime_t(const tm& t) : date{t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_wday},time{t.tm_hour,t.tm_min,t.tm_sec} {}
};
struct FakeRtc {
  uint8_t memory[128] = {};
  bool readable = true, writable = true, raw = false, ignoreCalendarWrite = false;
  int64_t epoch = 1789560000, setAt = 0;
  uint8_t getAddress() { return 0x32; }
  bool readRegister(uint8_t reg, uint8_t* data, size_t count) {
    if (!readable) return false;
    if (reg == 0x10 && !raw) {
      time_t value = epoch + testRtcTicks - setAt;
      tm utc;
      gmtime_r(&value, &utc);
      auto bcd=[](int v)->uint8_t{return (v/10)*16+v%10;};
      uint8_t calendar[]={bcd(utc.tm_sec),bcd(utc.tm_min),bcd(utc.tm_hour),uint8_t(1<<utc.tm_wday),bcd(utc.tm_mday),bcd(utc.tm_mon+1),bcd(utc.tm_year-100)};
      std::copy(calendar,calendar+7,memory+0x10);
    }
    std::copy(memory+reg,memory+reg+count,data);
    return true;
  }
  bool writeRegister(uint8_t reg, const uint8_t* data, size_t count) {
    if (!writable) return false;
    std::copy(data,data+count,memory+reg);
    return true;
  }
  bool writeRegister8(uint8_t reg,uint8_t value) {return writeRegister(reg,&value,1);}
  bool setDateTime(const rtc_date_t* date,const rtc_time_t* clock) {
    if (!writable) return false;
    if (ignoreCalendarWrite) return true;
    tm t={};t.tm_year=date->year-1900;t.tm_mon=date->month-1;t.tm_mday=date->date;
    t.tm_hour=clock->hours;t.tm_min=clock->minutes;t.tm_sec=clock->seconds;
    epoch=timegm(&t);setAt=testRtcTicks;raw=false;
    return true;
  }
};
}
struct RtcFake {
  bool enabled=true;
  m5::FakeRtc rtc;
  m5::FakeRtc* getRtcInstancePtr() { return enabled ? &rtc : nullptr; }
};
struct M5Fake { RtcFake Rtc; };
inline M5Fake M5;
