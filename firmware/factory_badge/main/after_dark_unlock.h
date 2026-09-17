#pragma once
#include "schedule.h"
#include <cstdint>

// Product policy only: the main task supplies checked time, physical Morse
// recognition and NVS persistence. No hardware or view ownership lives here.
namespace badge_after_dark {
inline constexpr int UnlockMinute = 13 * 60 + 30;

class Unlock {
public:
    static constexpr uint8_t SavedLocked = 0xA0;
    static constexpr uint8_t SavedUnlocked = 0xA1;
    static constexpr uint32_t SaveRetryMs = 5000;

    // The exact versioned byte is required. Missing/unknown data stays locked
    // without scheduling a write or changing any existing settings schema.
    bool restore(uint8_t stored) {
        unlocked_ = stored == SavedUnlocked;
        pending_ = false;
        saveAt_ = 0;
        return stored == SavedLocked || stored == SavedUnlocked;
    }

    bool unlocked() const { return unlocked_; }
    bool pending() const { return pending_; }
    uint8_t encoded() const { return unlocked_ ? SavedUnlocked : SavedLocked; }

    // A reveal is permanent for this badge. Midnight, a clock correction or a
    // later invalid RTC cannot take away a page the attendee already unlocked.
    bool unlock(uint32_t now) {
        if (unlocked_) return false;
        unlocked_ = true;
        pending_ = true;
        saveAt_ = now;
        return true;
    }

    bool updateClock(int64_t utc, int offsetMinutes, bool valid, uint32_t now) {
        if (unlocked_ || badge_schedule::localMinute(utc, offsetMinutes, valid) < UnlockMinute)
            return false;
        return unlock(now);
    }

    bool saveDue(uint32_t now) const {
        return pending_ && int32_t(now - saveAt_) >= 0;
    }
    void saved() { pending_ = false; }
    void saveFailed(uint32_t now) {
        if (pending_) saveAt_ = now + SaveRetryMs;
    }

private:
    bool unlocked_ = false;
    bool pending_ = false;
    uint32_t saveAt_ = 0;
};
} // namespace badge_after_dark
