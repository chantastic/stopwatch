#include "clock_service.h"
#include "board.h"
#include <nvs.h>
#include <sys/time.h>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <cstring>

namespace badge_clock {
namespace {
bool ready = false;
int utcOffset = 0;
const char* adoptedSource = "unset";
struct Request {
    int64_t epoch;
    int offset;
    bool done = false, success = false;
    std::string error;
    badge::ClockSnapshot snapshot;
};
std::mutex requestMutex;
std::condition_variable requestChanged;
std::shared_ptr<Request> pending;
bool epochValid(int64_t value) { return value >= MinEpoch && value < MaxEpoch; }
bool offsetValid(int value) { return value >= -840 && value <= 840; }
bool systemSet(int64_t value) {
    timeval tv = {};
    tv.tv_sec = static_cast<time_t>(value);
    return settimeofday(&tv, nullptr) == 0;
}
std::string format(const char* pattern, const char* empty) {
    if (!valid()) return empty;
    time_t local = static_cast<time_t>(epoch() + int64_t(utcOffset) * 60);
    tm fields = {};
    char text[40] = {};
    if (!gmtime_r(&local, &fields) || !strftime(text, sizeof(text), pattern, &fields)) return empty;
    return text;
}
}
void init() {
    nvs_handle_t handle;
    if (nvs_open("conf-clock", NVS_READONLY, &handle) == ESP_OK) {
        int32_t value = 0;
        if (nvs_get_i32(handle, "offset", &value) == ESP_OK && offsetValid(value)) utcOffset = value;
        nvs_close(handle);
    }
    int64_t value = 0;
    ready = board::readRtcUtc(value) && epochValid(value) && systemSet(value);
    adoptedSource = ready ? "rtc" : "unset";
}
bool valid() { return ready && epochValid(int64_t(time(nullptr))); }
int64_t epoch() { return valid() ? int64_t(time(nullptr)) : 0; }
int offset() { return utcOffset; }
const char* source() { return adoptedSource; }
std::string timeText() { return format("%H:%M", "--:--"); }
std::string dateText() { return format("%Y-%m-%d %H:%M", "Date / time not set"); }
bool validNonce(const char* value) {
    if (!value) return false;
    size_t size = strlen(value);
    if (size < 8 || size > 64) return false;
    for (size_t i = 0; i < size; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f'))) return false;
    return true;
}
bool set(int64_t value, int zone, const char* origin, std::string& error) {
    error.clear();
    if (!epochValid(value) || !offsetValid(zone)) { error = "invalid_value"; return false; }
    ready = false; adoptedSource = "unset";
    if (!board::setRtcUtc(value)) { error = "rtc_write_failed"; return false; }
    int64_t readback = 0;
    if (!board::readRtcUtc(readback) || readback < value || readback > value + 1) {
        error = "rtc_verify_failed"; return false;
    }
    nvs_handle_t handle;
    if (nvs_open("conf-clock", NVS_READWRITE, &handle) != ESP_OK) { error = "offset_store_failed"; return false; }
    int32_t previous = 9999;
    bool saved = nvs_get_i32(handle, "offset", &previous) == ESP_OK && previous == zone;
    if (!saved) saved = nvs_set_i32(handle, "offset", zone) == ESP_OK && nvs_commit(handle) == ESP_OK;
    int32_t verified = 9999;
    saved = saved && nvs_get_i32(handle, "offset", &verified) == ESP_OK && verified == zone;
    nvs_close(handle);
    if (!saved) { error = "offset_store_failed"; return false; }
    if (!systemSet(readback) || time(nullptr) < readback || time(nullptr) > readback + 1) {
        error = "system_clock_failed"; return false;
    }
    utcOffset = zone; adoptedSource = origin; ready = true;
    return true;
}
bool setFromPhone(int64_t value, int zone, badge::ClockSnapshot& snapshot, std::string& error) {
    snapshot = {};
    auto request = std::make_shared<Request>();
    request->epoch = value; request->offset = zone;
    std::unique_lock<std::mutex> lock(requestMutex);
    if (pending) { error = "clock_busy"; return false; }
    pending = request;
    bool done = requestChanged.wait_for(lock, std::chrono::seconds(8), [&]{ return request->done; });
    if (!done) {
        if (pending == request) pending.reset();
        error = "clock_timeout"; return false;
    }
    snapshot = request->snapshot;
    error = request->error;
    return request->success;
}
void poll() {
    std::shared_ptr<Request> request;
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        request = pending;
    }
    if (!request) return;
    std::string error;
    bool success = set(request->epoch, request->offset, "phone", error);
    badge::ClockSnapshot snapshot;
    int64_t rtc = 0;
    const bool rtcValid = board::readRtcUtc(rtc) && epochValid(rtc);
    snapshot.epoch = epoch();
    snapshot.rtc_epoch = rtcValid ? rtc : 0;
    snapshot.offset_minutes = utcOffset;
    snapshot.source = adoptedSource;
    snapshot.valid = success && valid() && rtcValid && snapshot.epoch >= rtc - 1 && snapshot.epoch <= rtc + 1;
    if (success && !snapshot.valid) {
        success = false;
        error = "clock_verify_failed";
        ready = false; adoptedSource = "unset";
        snapshot.epoch = 0; snapshot.source = "unset";
    }
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        request->snapshot = std::move(snapshot);
        request->success = success; request->error = error; request->done = true;
        if (pending == request) pending.reset();
    }
    requestChanged.notify_all();
}
}
