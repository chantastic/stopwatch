#pragma once
#include "clock_types.h"
#include <cstdint>
#include <string>

namespace badge_clock {
constexpr int64_t MinEpoch = 1704067200LL, MaxEpoch = 4102444800LL;
void init();
void poll();
bool valid();
int64_t epoch();
int offset();
const char* source();
std::string timeText(); // Local h:mm AM/PM; --:-- while invalid.
std::string dateText(); // Local YYYY-MM-DD h:mm AM/PM; placeholder while invalid.
// Main-task call; checks RTC write/readback, NVS offset and system time.
bool set(int64_t epoch, int offset, const char* source, std::string& error);
// HTTP-task call; marshals to main so all I2C and clock state have one owner.
bool setFromPhone(int64_t epoch, int offset, badge::ClockSnapshot& snapshot, std::string& error);
bool validNonce(const char* nonce);
}
