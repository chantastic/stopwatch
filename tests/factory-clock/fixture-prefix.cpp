// Host adapters; run.py inserts the actual native clock implementation below.
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <sys/time.h>
#include "@@ROOT@@/firmware/factory_badge/main/clock_service.h"
#include "@@ROOT@@/firmware/factory_badge/main/services.h"

static const auto main_thread = std::this_thread::get_id();
static int64_t wall = 1789670200, rtc = wall;
static bool rtc_ok = true, rtc_write_ok = true, nvs_ok = true, system_ok = true;
static bool has_offset = false, final_read_fails = false;
static int32_t stored_offset = 0;
static int writes = 0, rtc_reads = 0, write_adjustment = 0, final_read_skew = 0;
namespace board {
bool readRtcUtc(int64_t& value) {
    assert(std::this_thread::get_id() == main_thread);
    ++rtc_reads;
    value = rtc + (rtc_reads >= 2 ? final_read_skew : 0);
    return rtc_ok && !(final_read_fails && rtc_reads >= 2);
}
bool setRtcUtc(int64_t value) {
    assert(std::this_thread::get_id() == main_thread);
    if (!rtc_write_ok) return false;
    rtc = value + write_adjustment; rtc_reads = 0; return true;
}
}
using nvs_handle_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, NVS_READONLY = 0, NVS_READWRITE = 1;
int nvs_open(const char*, int, nvs_handle_t* handle) {
    assert(std::this_thread::get_id() == main_thread);
    *handle = 1; return nvs_ok ? 0 : -1;
}
int nvs_get_i32(nvs_handle_t, const char*, int32_t* value) { if (!has_offset) return -1; *value = stored_offset; return 0; }
int nvs_set_i32(nvs_handle_t, const char*, int32_t value) { stored_offset = value; has_offset = true; ++writes; return 0; }
int nvs_commit(nvs_handle_t) { return nvs_ok ? 0 : -1; }
void nvs_close(nvs_handle_t) {}
int host_settimeofday(const timeval* value, const void*) {
    assert(std::this_thread::get_id() == main_thread);
    if (!system_ok) return -1;
    wall = value->tv_sec; return 0;
}
time_t host_time(time_t*) { assert(std::this_thread::get_id() == main_thread); return wall; }
#define settimeofday host_settimeofday
#define time host_time
