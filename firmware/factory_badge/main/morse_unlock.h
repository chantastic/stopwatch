#pragma once

#include <cstdint>

// Recognizes a held/released input level for the whole word: ".. -. .. -".
// The invitation view feeds its native LVGL touch surface through the first
// input; the optional second input is still mutually exclusive for the word.
//
// A tap is 50–349 ms, a dash is 350–1400 ms, symbols are separated by 50–599 ms,
// and 600 ms quiet ends a letter. The exact final dash succeeds on release.
// A mismatch poisons recognition, but input() keeps the registered dots/dashes
// visible, grouped by single spaces. After 2500 ms with both inputs released,
// the attempt clears and restartCount() advances so the view can show feedback.
// Holds never time out as idle; the 15000 ms whole-attempt bound only poisons
// recognition. A correct-looking suffix cannot unlock a malformed attempt.
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
    static constexpr uint32_t ResetGapMs = 2500;
    static constexpr uint32_t MaxAttemptMs = 15000;
    static constexpr uint8_t InputCapacity = 32;

    // Includes a trailing space as soon as the most recent letter's gap ends.
    const char* input() const { return input_; }
    uint32_t restartCount() const { return restartCount_; }

    // If an input is already down, require full release before listening again.
    // Deliberate cancellation does not count as an automatic idle restart.
    void reset(bool yellow = false, bool blue = false) {
        clearAttempt();
        waitingForRelease_ = yellow || blue;
    }

    // Returns true exactly once per complete word, on the final dash's release.
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
        if (!pusher_) {
            if (held) beginAttempt(held, now);
            return false;
        }

        // Check the old release before beginning a new press: a tap arriving at
        // the deadline belongs to the next attempt and must not be swallowed.
        if (!pressed_ && elapsed(now, releasedAt_) >= ResetGapMs) {
            clearAttempt();
            ++restartCount_;
            if (held) beginAttempt(held, now);
            return false;
        }
        if (elapsed(now, attemptStarted_) >= MaxAttemptMs) rejected_ = true;
        if (held && held != pusher_) rejected_ = true;

        if (pressed_) {
            const uint32_t duration = elapsed(now, pressedAt_);
            if (duration > MaxHoldMs) rejected_ = true;
            if (held) return false;
            pressed_ = false;
            releasedAt_ = now;
            if (duration < MinHoldMs || duration > MaxHoldMs) {
                rejected_ = true;
                return false;
            }
            const char symbol = duration < DashThresholdMs ? '.' : '-';
            append(symbol);
            letterOpen_ = true;
            if (!rejected_) {
                const char* expected = letter(letterIndex_);
                if (!expected[symbolIndex_] || expected[symbolIndex_] != symbol) {
                    rejected_ = true;
                } else {
                    ++symbolIndex_;
                    if (letterIndex_ == 3 && expected[symbolIndex_] == '\0') {
                        reset();
                        return true;
                    }
                }
            }
            return false;
        }

        const uint32_t gap = elapsed(now, releasedAt_);
        if (letterOpen_ && gap >= LetterGapMs) {
            append(' ');
            letterOpen_ = false;
            if (!rejected_) {
                if (letter(letterIndex_)[symbolIndex_] != '\0') rejected_ = true;
                else ++letterIndex_;
                symbolIndex_ = 0;
            }
        }
        if (held) {
            if (gap < MinSymbolGapMs) rejected_ = true;
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
    void beginAttempt(uint8_t held, uint32_t now) {
        pusher_ = held;
        attemptStarted_ = pressedAt_ = now;
        pressed_ = true;
        rejected_ = held == 3;
    }
    void append(char symbol) {
        if (inputLength_ == InputCapacity) {
            rejected_ = true;
            return;
        }
        input_[inputLength_++] = symbol;
        input_[inputLength_] = '\0';
    }
    void clearAttempt() {
        pusher_ = letterIndex_ = symbolIndex_ = inputLength_ = 0;
        pressed_ = rejected_ = letterOpen_ = false;
        input_[0] = '\0';
    }

    uint32_t attemptStarted_ = 0, pressedAt_ = 0, releasedAt_ = 0;
    uint32_t restartCount_ = 0;
    char input_[InputCapacity + 1] = {};
    uint8_t pusher_ = 0, letterIndex_ = 0, symbolIndex_ = 0, inputLength_ = 0;
    bool pressed_ = false, waitingForRelease_ = false;
    bool rejected_ = false, letterOpen_ = false;
};
