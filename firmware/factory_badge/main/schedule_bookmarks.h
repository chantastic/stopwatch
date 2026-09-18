#pragma once
#include "schedule.h"
#include <cstdint>

namespace badge_schedule {
// Bookmark policy stays independent of the daily agenda clock. Main owns the
// separate NVS value; a view only requests a toggle by stable agenda index.
class Bookmarks {
public:
    static_assert(Items.size() <= 16, "Bookmark storage has sixteen agenda bits");
    static constexpr uint16_t ValidMask = uint16_t((uint32_t(1) << Items.size()) - 1);
    static constexpr uint32_t Version = 0xb5010000u;
    static constexpr uint32_t SaveDelayMs = 1200;
    static constexpr uint32_t SaveRetryMs = 5000;

    uint16_t mask() const { return mask_; }
    bool bookmarked(int index) const {
        return index >= 0 && index < int(Items.size()) && (mask_ & (uint16_t(1) << index));
    }
    bool pending() const { return pending_; }
    uint32_t encoded() const { return Version | mask_; }
    bool restore(uint32_t stored) {
        mask_ = 0;
        pending_ = false;
        changed_at_ = 0;
        wait_ms_ = SaveDelayMs;
        if ((stored & ~uint32_t(ValidMask)) != Version) return false;
        mask_ = uint16_t(stored & ValidMask);
        return true;
    }
    bool toggle(int index, uint32_t now) {
        if (index < 0 || index >= int(Items.size())) return false;
        mask_ ^= uint16_t(1) << index;
        pending_ = true;
        changed_at_ = now;
        wait_ms_ = SaveDelayMs;
        return true;
    }
    bool saveDue(uint32_t now) const {
        return pending_ && uint32_t(now - changed_at_) >= wait_ms_;
    }
    void saved() { pending_ = false; }
    void saveFailed(uint32_t now) {
        if (!pending_) return;
        changed_at_ = now;
        wait_ms_ = SaveRetryMs;
    }

private:
    uint16_t mask_ = 0;
    bool pending_ = false;
    uint32_t changed_at_ = 0;
    uint32_t wait_ms_ = SaveDelayMs;
};
} // namespace badge_schedule
