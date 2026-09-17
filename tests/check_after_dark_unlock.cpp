#include "../firmware/factory_badge/main/after_dark_unlock.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <limits>

int main() {
    using badge_after_dark::Unlock;
    // Calendar fields are expressed on a UTC-shaped axis here; updateClock
    // must apply the badge's saved offset to the actual supplied UTC instant.
    constexpr int64_t midnight = 1791331200;  // 2026-10-07 00:00 calendar fields.
    constexpr int64_t threshold = 1791379800; // 2026-10-07 13:30 calendar fields.
    constexpr int64_t minEpoch = 1704067200;  // 2024-01-01 UTC, inclusive.
    constexpr int64_t maxEpoch = 4102444800;  // 2100-01-01 UTC, exclusive.
    static_assert(Unlock::SavedUnlocked == 0xA1); // Preserve already earned unlocks.

    Unlock state;
    assert(!state.unlocked() && !state.pending() && !state.saveDue(0));
    assert(!state.updateClock(threshold, 0, false, 1));
    assert(!state.updateClock(0, 0, true, 2));
    assert(!state.updateClock(-1, 0, true, 3));
    assert(!state.updateClock(threshold, 841, true, 4));
    assert(!state.updateClock(threshold, -841, true, 5));
    assert(!state.updateClock(threshold, std::numeric_limits<int>::min(), true, 5));
    assert(!state.updateClock(threshold, std::numeric_limits<int>::max(), true, 5));
    assert(!state.pending());
    for (int64_t utc : {std::numeric_limits<int64_t>::min(), int64_t(-1), int64_t(0),
                        minEpoch - 1, maxEpoch, maxEpoch + 1, std::numeric_limits<int64_t>::max()}) {
        for (int offset : {-840, 0, 840}) {
            Unlock invalid;
            assert(!invalid.updateClock(utc, offset, true, 1));
            assert(!invalid.unlocked() && !invalid.pending());
        }
    }
    assert(!state.updateClock(threshold - 1, 0, true, 6));
    assert(state.updateClock(threshold, 0, true, 7));
    assert(state.unlocked() && state.pending() && state.saveDue(7));
    assert(!state.updateClock(threshold + 1, 0, true, 8));
    assert(!state.unlock(9));

    // Save failures leave the page visible and dirty, and use monotonic retry
    // deadlines rather than a phone-adjustable wall clock.
    state.saveFailed(10);
    assert(!state.saveDue(10) && !state.saveDue(5009) && state.saveDue(5010));
    assert(!state.updateClock(midnight, 0, true, 11));
    assert(!state.updateClock(0, 0, false, 12));
    assert(state.unlocked() && state.pending() && !state.saveDue(13));
    state.saveFailed(5010);
    assert(!state.saveDue(10009) && state.saveDue(10010));
    state.saved();
    assert(!state.pending() && !state.saveDue(10010));
    assert(!state.updateClock(threshold, 0, true, 10011));
    assert(!state.unlock(10012));
    assert(!state.pending());
    state.saveFailed(10013); // A stale failure cannot dirty a completed save.
    assert(!state.saveDue(15013));

    // A committed unlock survives a boot before lunch or with no valid RTC.
    Unlock reboot;
    assert(reboot.restore(state.encoded()));
    assert(reboot.unlocked() && !reboot.pending());
    assert(!reboot.updateClock(midnight + 8 * 3600, 0, true, 1));
    assert(!reboot.updateClock(0, 0, false, 2));
    assert(reboot.unlocked() && !reboot.saveDue(3));
    assert(!reboot.updateClock(midnight + 86400, 0, true, 4));
    assert(reboot.unlocked()); // Daily agenda reset never re-locks the invite.

    // All possible bytes are bounded. Only the exact recognized encoding can
    // restore an unlock; corrupt/unknown values neither unlock nor rewrite NVS.
    for (int byte = 0; byte <= 255; ++byte) {
        Unlock decoded;
        assert(decoded.restore(uint8_t(byte)) ==
               (byte == Unlock::SavedLocked || byte == Unlock::SavedUnlocked));
        assert(decoded.unlocked() == (byte == Unlock::SavedUnlocked));
        assert(!decoded.pending() && !decoded.saveDue(0));
    }

    // The release is a fixed date/time, not an afternoon-only daily window.
    // Every minute before the event stays locked (including earlier evenings),
    // and every later minute is eligible even on a fresh badge with no history.
    for (int day : {-365, -31, -1, 0, 1, 2, 365, 366, 730}) {
        for (int minute = 0; minute < 1440; ++minute) {
            Unlock fresh;
            const bool due = day > 0 || (day == 0 && minute >= 13 * 60 + 30);
            assert(fresh.updateClock(midnight + int64_t(day) * 86400 + minute * 60,
                                     0, true, 10) == due);
            assert(fresh.unlocked() == due && fresh.pending() == due);
        }
    }
    for (int offset : {-840, -480, -420, 0, 330, 840}) {
        Unlock local;
        const int64_t localThreshold = threshold - int64_t(offset) * 60;
        assert(!local.updateClock(localThreshold - 1, offset, true, 1));
        assert(local.updateClock(localThreshold, offset, true, 2));
        for (int hour = 0; hour < 24; ++hour) {
            Unlock followingDay;
            const int64_t utc = midnight + 86400 + hour * 3600 - int64_t(offset) * 60;
            assert(followingDay.updateClock(utc, offset, true, 3));
        }
    }
    Unlock forward;
    assert(!forward.updateClock(midnight - 86400 + 23 * 3600, 0, true, 1));
    assert(forward.updateClock(midnight + 17 * 3600, 0, true, 2));
    assert(!forward.updateClock(midnight - 86400, 0, true, 3));
    assert(forward.unlocked()); // Correcting the date backward cannot re-lock.
    Unlock west;
    assert(!west.updateClock(midnight + 4 * 3600 + 30 * 60, -420, true, 1)); // Oct 6, 21:30 local.
    Unlock east;
    assert(east.updateClock(midnight + 23 * 3600 + 30 * 60, 330, true, 1)); // Oct 8, 05:00 local.
    Unlock minimum;
    assert(!minimum.updateClock(minEpoch, 840, true, 1));
    Unlock maximum;
    assert(maximum.updateClock(maxEpoch - 1, 840, true, 1)); // Valid UTC, local date crosses 2100.

    // Morse can reveal at any clock time, including when the RTC is invalid.
    Unlock morse;
    assert(morse.unlock(0) && morse.saveDue(0));
    assert(morse.encoded() == Unlock::SavedUnlocked);
    assert(!morse.updateClock(0, 0, false, 1) && morse.unlocked());

    // Millisecond counter wrap does not suppress an immediate save or shorten
    // the retry interval. Repeated reveal attempts do not extend the retry.
    Unlock wrap;
    constexpr uint32_t start = UINT32_MAX - 2000;
    assert(wrap.unlock(start) && wrap.saveDue(start));
    wrap.saveFailed(start);
    const uint32_t retry = start + Unlock::SaveRetryMs;
    assert(!wrap.saveDue(start + 1));
    assert(!wrap.unlock(start + 2));
    assert(!wrap.saveDue(retry - 1) && wrap.saveDue(retry));
    wrap.saved();
    assert(!wrap.pending() && !wrap.saveDue(retry + 1));

    std::puts("After Dark unlock: October 7 2026 1:30 PM local boundary, all later dates, offsets, invalid clocks, persistent latch, versioned encoding and monotonic retry passed");
}
