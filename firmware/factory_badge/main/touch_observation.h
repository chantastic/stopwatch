#pragma once

#include <cstddef>
#include <cstdint>

namespace badge_touch {

// Explicitly enabled, main-task-only diagnostics. This records sensor contacts,
// not LVGL events or Morse decisions. No allocation, persistence, or I/O occurs.
class Observation {
 public:
    static constexpr size_t Capacity = 64;
    static constexpr uint32_t MaxDurationMs = 120000;

    struct Contact {
        enum class End : uint8_t { Released, Lost, Ineligible, Expired, Stopped };
        uint32_t press_ms = 0, release_ms = 0, duration_ms = 0;
        uint32_t gap_before_ms = 0;
        int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
        uint32_t max_dx = 0, max_dy = 0;
        bool has_previous = false;
        End end = End::Released;

        const char* reason() const {
            switch (end) {
                case End::Released: return "release";
                case End::Lost: return "lost";
                case End::Ineligible: return "ineligible";
                case End::Expired: return "expired";
                case End::Stopped: return "stopped";
            }
            return "lost";
        }
    };

    // A new session discards the previous RAM buffer. Require a real released
    // sample before its first contact so an already-held finger is not a tap.
    void start(uint32_t now, uint32_t durationMs = MaxDurationMs) {
        head_ = count_ = 0;
        dropped_ = 0;
        started_ = now;
        duration_ = durationMs > MaxDurationMs ? MaxDurationMs : durationMs;
        enabled_ = duration_ != 0;
        holding_ = eligible_ = previous_ = false;
        wait_release_ = true;
    }

    // Keep buffered records available after stop/expiry. A held contact is
    // explicitly cancelled, never reported as a measured physical release.
    void stop(uint32_t now) {
        if (!active(now)) return;
        if (holding_) finish(Contact::End::Stopped, now);
        enabled_ = eligible_ = previous_ = false;
        wait_release_ = true;
    }

    bool active(uint32_t now) {
        if (!enabled_) return false;
        const uint32_t elapsed = now - started_;
        if (elapsed < 0x80000000u && elapsed >= duration_) {
            // This is the diagnostic deadline, not an observed finger lift.
            if (holding_) finish(Contact::End::Expired, started_ + duration_);
            enabled_ = eligible_ = previous_ = false;
            wait_release_ = true;
        }
        return enabled_;
    }

    // Feed every fresh physical sample, including movement and failed reads.
    // x/y are displayed coordinates; the caller owns the page/modal scope.
    // Injection cancels an open record and cannot supply a physical release.
    // A cancelled record's release_ms is its termination time; reason() tells
    // consumers whether that time was an actual observed release.
    void observe(bool eligible, bool valid, bool pressed, bool sensor,
                 int x, int y, uint32_t sampledAtMs) {
        // A cached sample from before start must not expire a fresh session.
        if (uint32_t(sampledAtMs - started_) >= 0x80000000u ||
            !active(sampledAtMs)) return;
        if (!eligible) {
            if (holding_) finish(Contact::End::Ineligible, sampledAtMs);
            eligible_ = previous_ = false;
            wait_release_ = true;
            return;
        }
        if (!eligible_) {
            eligible_ = true;
            previous_ = false;
            wait_release_ = true;
        }
        if (!valid || !sensor) {
            if (holding_) finish(Contact::End::Lost, sampledAtMs);
            previous_ = false;
            wait_release_ = true;
            return;
        }
        if (wait_release_) {
            if (!pressed) wait_release_ = false;
            return;
        }
        if (!holding_) {
            if (!pressed) return;
            current_ = Contact{};
            current_.press_ms = sampledAtMs;
            current_.start_x = current_.end_x = x;
            current_.start_y = current_.end_y = y;
            current_.has_previous = previous_;
            if (previous_) current_.gap_before_ms = sampledAtMs - previous_release_;
            holding_ = true;
            return;
        }
        point(x, y);
        if (!pressed) finish(Contact::End::Released, sampledAtMs);
    }

    bool pop(Contact& result) {
        if (!count_) return false;
        result = records_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }

    size_t count() const { return count_; }
    uint32_t dropped() const { return dropped_; }

 private:
    static uint32_t distance(int a, int b) {
        const int64_t delta = int64_t(a) - int64_t(b);
        return static_cast<uint32_t>(delta < 0 ? -delta : delta);
    }
    void point(int x, int y) {
        current_.end_x = x;
        current_.end_y = y;
        const uint32_t dx = distance(x, current_.start_x);
        const uint32_t dy = distance(y, current_.start_y);
        if (dx > current_.max_dx) current_.max_dx = dx;
        if (dy > current_.max_dy) current_.max_dy = dy;
    }
    void finish(Contact::End reason, uint32_t now) {
        current_.release_ms = now;
        current_.duration_ms = now - current_.press_ms;
        current_.end = reason;
        // Retain the most recent bounded observations; report any older loss.
        if (count_ == Capacity) {
            head_ = (head_ + 1) % Capacity;
            --count_;
            ++dropped_;
        }
        records_[(head_ + count_) % Capacity] = current_;
        ++count_;
        holding_ = false;
        previous_ = reason == Contact::End::Released;
        if (previous_) previous_release_ = now;
    }

    Contact records_[Capacity]{};
    Contact current_{};
    size_t head_ = 0, count_ = 0;
    uint32_t started_ = 0, duration_ = 0, previous_release_ = 0, dropped_ = 0;
    bool enabled_ = false, holding_ = false, eligible_ = false;
    bool previous_ = false, wait_release_ = true;
};

}  // namespace badge_touch
