#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "../firmware/factory_badge/main/morse_unlock.h"

namespace {
struct Input {
    MorseUnlock recognizer;
    uint32_t now = 0;
    unsigned unlocks = 0;
    bool blue = false;
    bool sample(uint32_t after, bool yellowHeld, bool blueHeld, bool enabled = true) {
        now += after;
        const bool result = recognizer.update(yellowHeld, blueHeld, now, enabled);
        if (result) ++unlocks;
        return result;
    }
    bool quiet(uint32_t ms) { return sample(ms, false, false); }
    void down() { assert(!sample(0, !blue, blue)); }
    void pulse(uint32_t hold, uint32_t gap) {
        down();
        assert(!sample(hold, false, false));
        quiet(gap);
    }
    void word(uint32_t dot = 150, uint32_t dash = 650,
              uint32_t innerGap = 150, uint32_t letterGap = 800) {
        pulse(dot, innerGap); pulse(dot, letterGap);  // i
        pulse(dash, innerGap); pulse(dot, letterGap); // n
        pulse(dot, innerGap); pulse(dot, letterGap);  // i
        pulse(dash, MorseUnlock::LetterGapMs);         // t
    }
    void firstThreeLetters() {
        pulse(150, 150); pulse(150, 800);
        pulse(650, 150); pulse(150, 800);
        pulse(150, 150); pulse(150, 800);
    }
};

void validWords() {
    for (bool blue : {false, true}) {
        Input r; r.blue = blue;
        r.word(); assert(r.unlocks == 1);
        for (unsigned i = 0; i != 100; ++i) assert(!r.quiet(100));
        r.word(); assert(r.unlocks == 2);
    }
    Input fastest;
    fastest.word(MorseUnlock::MinHoldMs, MorseUnlock::DashThresholdMs,
                 MorseUnlock::MinSymbolGapMs, MorseUnlock::LetterGapMs);
    assert(fastest.unlocks == 1);
    Input slow;
    slow.word(MorseUnlock::DashThresholdMs - 1, MorseUnlock::MaxHoldMs,
              MorseUnlock::LetterGapMs - 1, 1000);
    assert(slow.unlocks == 1);
    Input wrapped;
    wrapped.now = UINT32_MAX - 100;
    wrapped.word(); assert(wrapped.unlocks == 1);
}

void exactLettersAndFinalPause() {
    Input r;
    r.firstThreeLetters();
    r.pulse(650, MorseUnlock::LetterGapMs - 1);
    assert(r.unlocks == 0);
    assert(r.quiet(1)); assert(r.unlocks == 1);

    Input missingLetterPause;
    for (char c : "..-...-") {
        if (c) missingLetterPause.pulse(c == '.' ? 150 : 650, 150);
    }
    missingLetterPause.quiet(800);
    assert(missingLetterPause.unlocks == 0);

    Input prematurePause;
    prematurePause.pulse(150, 800); // e instead of first i
    prematurePause.word(); assert(prematurePause.unlocks == 0);
    prematurePause.quiet(MorseUnlock::ResetGapMs);
    prematurePause.word(); assert(prematurePause.unlocks == 1);

    Input extraFinalSymbol;
    extraFinalSymbol.firstThreeLetters();
    extraFinalSymbol.pulse(650, 150);
    extraFinalSymbol.pulse(150, 800); // n instead of t
    assert(extraFinalSymbol.unlocks == 0);
    extraFinalSymbol.word(); assert(extraFinalSymbol.unlocks == 0);

    Input wrongDash;
    wrongDash.pulse(650, 800); // t before a correct-looking suffix
    wrongDash.word(); assert(wrongDash.unlocks == 0);
}

void holdsNoiseAndTimeouts() {
    Input noise;
    noise.pulse(MorseUnlock::MinHoldMs - 1, 150);
    noise.word(); assert(noise.unlocks == 0);

    Input tooClose;
    tooClose.pulse(150, MorseUnlock::MinSymbolGapMs - 1);
    tooClose.pulse(150, 800);
    tooClose.word(); assert(tooClose.unlocks == 0);

    Input held;
    held.down();
    assert(!held.sample(MorseUnlock::MaxHoldMs + 1, true, false));
    assert(!held.sample(10000, true, false));
    held.quiet(0); held.quiet(MorseUnlock::ResetGapMs);
    held.word(); assert(held.unlocks == 1);

    Input lateRelease;
    lateRelease.pulse(MorseUnlock::MaxHoldMs + 1, 800);
    lateRelease.word(); assert(lateRelease.unlocks == 0);

    Input timeout;
    timeout.pulse(150, 150); timeout.pulse(150, MorseUnlock::ResetGapMs);
    timeout.pulse(650, 150); timeout.pulse(150, 800); // old continuation
    timeout.pulse(150, 150); timeout.pulse(150, 800); timeout.pulse(650, 800);
    assert(timeout.unlocks == 0);
    timeout.quiet(MorseUnlock::ResetGapMs);
    timeout.word(); assert(timeout.unlocks == 1);

    Input totalLimit;
    totalLimit.word(349, 1400, 599, 2900); // Each gap legal, whole attempt too long.
    assert(totalLimit.unlocks == 0);
}

void ownershipAndCancellation() {
    Input switched;
    switched.pulse(150, 150);
    switched.blue = true;
    switched.word(); assert(switched.unlocks == 0);
    switched.quiet(MorseUnlock::ResetGapMs);
    switched.word(); assert(switched.unlocks == 1);

    Input overlap;
    overlap.down();
    assert(!overlap.sample(100, true, true));
    assert(!overlap.sample(100, false, true));
    assert(!overlap.sample(100, true, false));
    overlap.quiet(100); overlap.word(); assert(overlap.unlocks == 0);
    overlap.quiet(MorseUnlock::ResetGapMs);
    overlap.word(); assert(overlap.unlocks == 1);

    Input disabled;
    disabled.firstThreeLetters();
    assert(!disabled.sample(0, false, false, false));
    disabled.pulse(650, 800); assert(disabled.unlocks == 0);
    disabled.quiet(MorseUnlock::ResetGapMs);
    disabled.word(); assert(disabled.unlocks == 1);

    Input disabledHeld;
    assert(!disabledHeld.sample(0, true, false, false));
    assert(!disabledHeld.sample(600, true, false));
    assert(!disabledHeld.sample(100, false, false));
    disabledHeld.word(); assert(disabledHeld.unlocks == 1);

    Input resetHeld;
    resetHeld.firstThreeLetters();
    resetHeld.recognizer.reset(true, false);
    assert(!resetHeld.sample(600, true, false));
    resetHeld.quiet(100);
    resetHeld.word(); assert(resetHeld.unlocks == 1);

    Input directSwitch;
    directSwitch.down();
    assert(!directSwitch.sample(100, false, true));
    directSwitch.quiet(100); directSwitch.word();
    assert(directSwitch.unlocks == 0);
}

void exhaustiveSevenSymbolWords() {
    // All 128 short/long patterns and all 64 ways of grouping seven pulses into
    // letters. Only the exact four letters may succeed, never a substring.
    for (unsigned symbols = 0; symbols != 128; ++symbols) {
        for (unsigned gaps = 0; gaps != 64; ++gaps) {
            Input r;
            for (unsigned i = 0; i != 7; ++i) {
                const auto hold = symbols & (1u << i) ? 650 : 150;
                const auto gap = i == 6 || (gaps & (1u << i)) ? 800 : 150;
                r.pulse(hold, gap);
            }
            const bool exactInit = symbols == ((1u << 2) | (1u << 6)) &&
                                   gaps == ((1u << 1) | (1u << 3) | (1u << 5));
            assert(r.unlocks == unsigned(exactInit));
        }
    }
}
} // namespace

int main() {
    validWords();
    exactLettersAndFinalPause();
    holdsNoiseAndTimeouts();
    ownershipAndCancellation();
    exhaustiveSevenSymbolWords();
    std::puts("Morse unlock: exact init word, either pusher, timing, cancellation and wraparound passed");
}
