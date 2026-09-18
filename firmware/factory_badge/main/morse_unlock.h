#pragma once

#include <cstdint>

// Recognizes a held/released input level for the whole word: ".. -. .. -".
// The invitation view feeds its native LVGL touch surface through the first
// input; the optional second input is still mutually exclusive for the word.
//
// Timings are deliberately human-sized: a tap is 50–349 ms, a dash is 350–1400
// ms, symbols are separated by 50–599 ms, and a letter ends after 600 ms quiet.
// The next letter must start before 3000 ms quiet; the entire attempt is bounded
// to 15000 ms. A mismatch poisons the attempt until 3000 ms with both released,
// so a correct-looking suffix inside a longer/malformed word cannot unlock it.
// The final dash also needs its letter pause: a prefix of another letter fails.
//
// Feed monotonic uint32 milliseconds, never the adjustable wall clock. Unsigned
// elapsed arithmetic handles wraparound. This class has no hardware, UI, storage,
// allocation, or framework dependencies. Reset/disable it around input modals.
class MorseUnlock {
 public:
    static constexpr uint32_t MinHoldMs = 50;
    static constexpr uint32_t DashThresholdMs = 350;
    static constexpr uint32_t MaxHoldMs = 1400;
    static constexpr uint32_t MinSymbolGapMs = 50;
    static constexpr uint32_t LetterGapMs = 600;
    static constexpr uint32_t ResetGapMs = 3000;
    static constexpr uint32_t MaxAttemptMs = 15000;

    // If a button is already down, require full release before listening again.
    void reset(bool yellow = false, bool blue = false) {
        clearAttempt();
        rejected_ = false;
        waitingForRelease_ = yellow || blue;
    }

    // Returns true exactly once per complete word, after the final letter gap.
    // enabled=false discards all progress and never starts a partial held press.
    bool update(bool yellow, bool blue, uint32_t now, bool enabled = true) {
        const uint8_t held = (yellow ? 1u : 0u) | (blue ? 2u : 0u);
        if (!enabled) {
            reset(yellow, blue);
            return false;
        }
        if (waitingForRelease_) {
            if (!held) waitingForRelease_ = false;
            return false;
        }
        if (rejected_) {
            if (held) quietAfterRejection_ = false;
            else if (!quietAfterRejection_) {
                quietAfterRejection_ = true;
                quietSince_ = now;
            } else if (elapsed(now, quietSince_) >= ResetGapMs) reset();
            return false;
        }
        if (held == 3) return reject(held, now);

        if (!pusher_) {
            if (!held) return false;
            pusher_ = held;
            attemptStarted_ = pressedAt_ = now;
            pressed_ = true;
            return false;
        }
        if (elapsed(now, attemptStarted_) >= MaxAttemptMs) return reject(held, now);
        if (held && held != pusher_) return reject(held, now);

        if (pressed_) {
            const uint32_t duration = elapsed(now, pressedAt_);
            if (duration > MaxHoldMs) return reject(held, now);
            if (held) return false;
            if (duration < MinHoldMs) return reject(held, now);
            const char symbol = duration < DashThresholdMs ? '.' : '-';
            const char* expected = letter(letterIndex_);
            if (!expected[symbolIndex_] || expected[symbolIndex_] != symbol)
                return reject(held, now);
            ++symbolIndex_;
            pressed_ = false;
            releasedAt_ = now;
            return false;
        }

        const uint32_t gap = elapsed(now, releasedAt_);
        if (gap >= ResetGapMs) {
            // An idle timeout closes the old attempt. A new press may start a
            // fresh word, but none of the old symbols are retained.
            reset(yellow, blue);
            if (held) {
                waitingForRelease_ = false;
                pusher_ = held;
                attemptStarted_ = pressedAt_ = now;
                pressed_ = true;
            }
            return false;
        }
        if (symbolIndex_ && gap >= LetterGapMs) {
            if (letter(letterIndex_)[symbolIndex_] != '\0') return reject(held, now);
            ++letterIndex_;
            symbolIndex_ = 0;
            if (letterIndex_ == 4) {
                reset(yellow, blue);
                return true;
            }
        }
        if (held) {
            if (gap < MinSymbolGapMs) return reject(held, now);
            pressed_ = true;
            pressedAt_ = now;
        }
        return false;
    }

 private:
    static uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
    static const char* letter(uint8_t index) {
        // Caller maintains index < 4; no substring or sliding-window matching.
        constexpr const char* letters[] = {"..", "-.", "..", "-"};
        return letters[index];
    }
    void clearAttempt() {
        pusher_ = letterIndex_ = symbolIndex_ = 0;
        pressed_ = false;
    }
    bool reject(uint8_t held, uint32_t now) {
        clearAttempt();
        rejected_ = true;
        quietAfterRejection_ = !held;
        quietSince_ = now;
        return false;
    }

    uint32_t attemptStarted_ = 0, pressedAt_ = 0, releasedAt_ = 0, quietSince_ = 0;
    uint8_t pusher_ = 0, letterIndex_ = 0, symbolIndex_ = 0;
    bool pressed_ = false, waitingForRelease_ = false;
    bool rejected_ = false, quietAfterRejection_ = false;
};
