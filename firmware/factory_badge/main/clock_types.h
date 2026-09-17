#pragma once
#include <cstdint>
#include <string>

namespace badge {
// An immutable result copied from the main task after hardware/system readback.
// HTTP workers serialize this value; they never read the RTC or live clock state.
struct ClockSnapshot {
    bool valid = false;
    int64_t epoch = 0;
    int64_t rtc_epoch = 0;
    int offset_minutes = 0;
    std::string source = "unset";
};
} // namespace badge
