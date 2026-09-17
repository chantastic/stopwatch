#pragma once
#include "clock_types.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace badge {
struct Profile { std::string name; std::string urls[3]; };
struct ProfileSnapshot {
  Profile profile;
  // Native little-endian RGB565, 160 x 160; immutable across UI frames.
  std::shared_ptr<const std::vector<uint16_t>> avatar;
  uint32_t revision = 0;
  bool ready = false;
  std::string error;
};
enum class PortalOutcome : uint8_t { None, Saved, Cancelled, TimedOut, Error };
struct PortalSnapshot {
  bool active = false, starting = false;
  std::string ssid, password, error;
  PortalOutcome outcome = PortalOutcome::None;
  uint8_t clients = 0;
};
// Callback runs on the HTTP task; board/clock implementation must serialize RTC
// access and return success only after RTC, system clock and NVS readback.
using PhoneClockSync = std::function<bool(int64_t, int, ClockSnapshot&, std::string&)>;
bool services_init(PhoneClockSync clock_sync);
ProfileSnapshot profile_snapshot();
// Asynchronous AP lifecycle: inspect starting/active/error in portal_snapshot().
bool portal_start(const char* test_password = nullptr);
void portal_stop();
void portal_tick(); // Compatibility hook; lifecycle is owned by a service task.
PortalSnapshot portal_snapshot();
bool profile_clear(); // Explicit diagnostic only; never called by normal startup.
// Destructive batch opt-in only: caller must validate the exact confirmation
// token and fresh nonce. Preserves mounted stores; never invoked implicitly.
bool profile_initialize_for_conference();
} // namespace badge
