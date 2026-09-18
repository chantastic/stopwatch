#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
    void down(uint32_t after = 0) { assert(!sample(after, !blue, blue)); }
    bool release(uint32_t hold) { return sample(hold, false, false); }
    bool pulse(uint32_t hold, uint32_t gap) {
        down();
        const bool result = release(hold);
        assert(!quiet(gap));
        return result;
    }
    void expect(const char* text) const { assert(!std::strcmp(recognizer.input(), text)); }
    void word(uint32_t dot = 150, uint32_t dash = 650,
              uint32_t innerGap = 150, uint32_t letterGap = 800) {
        pulse(dot, innerGap); pulse(dot, letterGap);  // i
        pulse(dash, innerGap); pulse(dot, letterGap); // n
        pulse(dot, innerGap); pulse(dot, letterGap);  // i
        pulse(dash, MorseUnlock::LetterGapMs);        // t
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
        r.word(); assert(r.unlocks == 1); r.expect("");
        for (unsigned i = 0; i != 100; ++i) assert(!r.quiet(100));
        assert(r.recognizer.restartCount() == 0);
        r.word(); assert(r.unlocks == 2);
    }
    Input fastest;
    fastest.word(MorseUnlock::MinHoldMs, MorseUnlock::DashThresholdMs,
                 MorseUnlock::MinSymbolGapMs, MorseUnlock::LetterGapMs);
    assert(fastest.unlocks == 1);
    Input slow;
    slow.word(MorseUnlock::DashThresholdMs - 1, MorseUnlock::MaxHoldMs,
              MorseUnlock::LetterGapMs - 1, MorseUnlock::ResetGapMs - 1);
    assert(slow.unlocks == 1);
    Input wrapped;
    wrapped.now = UINT32_MAX - 100;
    wrapped.word(); assert(wrapped.unlocks == 1);
}

void eagerFinalDashAndExactEarlierLetters() {
    Input r;
    r.firstThreeLetters(); r.expect(".. -. .. ");
    r.down();
    assert(!r.sample(MorseUnlock::DashThresholdMs, true, false));
    r.expect(".. -. .. "); // A held contact has not registered a symbol yet.
    assert(r.release(0)); assert(r.unlocks == 1); r.expect("");
    assert(!r.quiet(0)); assert(!r.quiet(MorseUnlock::ResetGapMs));
    assert(r.recognizer.restartCount() == 0);

    Input missingLetterPause;
    for (char c : "..-...-") {
        if (c) missingLetterPause.pulse(c == '.' ? 150 : 650, 150);
    }
    missingLetterPause.quiet(800);
    missingLetterPause.expect("..-...- ");
    assert(missingLetterPause.unlocks == 0);

    Input prematurePause;
    prematurePause.pulse(150, 800); // e instead of first i
    prematurePause.word(); assert(prematurePause.unlocks == 0);
    prematurePause.expect(". .. -. .. - ");
    prematurePause.quiet(MorseUnlock::ResetGapMs);
    prematurePause.word(); assert(prematurePause.unlocks == 1);

    Input wrongDash;
    wrongDash.pulse(650, 800); // t before a correct-looking suffix
    wrongDash.word(); assert(wrongDash.unlocks == 0);
    wrongDash.expect("- .. -. .. - ");

    // Eager success is final. Extra input after the complete code starts a new
    // attempt; it cannot retroactively turn the successful t into another letter.
    Input extraAfterSuccess;
    extraAfterSuccess.firstThreeLetters();
    assert(extraAfterSuccess.pulse(650, 150));
    extraAfterSuccess.pulse(150, 800);
    assert(extraAfterSuccess.unlocks == 1); extraAfterSuccess.expect(". ");
}

void visibleRegisteredSymbols() {
    Input r;
    r.down(); r.expect("");
    assert(!r.release(MorseUnlock::MinHoldMs)); r.expect(".");
    r.quiet(MorseUnlock::LetterGapMs - 1); r.expect(".");
    r.quiet(1); r.expect(". ");
    r.quiet(100); r.expect(". "); // Add one separator, never repeated spaces.
    r.pulse(MorseUnlock::DashThresholdMs - 1, 50); r.expect(". .");
    r.pulse(MorseUnlock::DashThresholdMs, 50); r.expect(". .-");
    r.pulse(MorseUnlock::MaxHoldMs, 600); r.expect(". .-- ");
    assert(r.unlocks == 0); // Incorrect input still shows every valid symbol.

    r.pulse(MorseUnlock::MinHoldMs - 1, 50); r.expect(". .-- ");
    r.pulse(MorseUnlock::MaxHoldMs + 1, 50); r.expect(". .-- ");
    r.pulse(150, 50); r.expect(". .-- .");
    r.pulse(150, 600); r.expect(". .-- .. ");
    assert(r.unlocks == 0);

    Input full;
    for (unsigned i = 0; i < MorseUnlock::InputCapacity + 20; ++i) full.pulse(50, 50);
    assert(std::strlen(full.recognizer.input()) == MorseUnlock::InputCapacity);
    for (unsigned i = 0; i < MorseUnlock::InputCapacity; ++i)
        assert(full.recognizer.input()[i] == '.');
    full.quiet(600); // A separator also cannot exceed the bounded buffer.
    assert(std::strlen(full.recognizer.input()) == MorseUnlock::InputCapacity);
    full.word(); assert(full.unlocks == 0);
    full.quiet(MorseUnlock::ResetGapMs); full.expect("");
    full.word(); assert(full.unlocks == 1);
}

void idleTimeoutAndImmediateRetry() {
    for (uint32_t late : {MorseUnlock::ResetGapMs, MorseUnlock::ResetGapMs + 1000}) {
        Input r;
        r.pulse(650, 0); r.expect("-");
        r.down(late); r.expect("");
        assert(r.recognizer.restartCount() == 1);
        assert(!r.release(150)); r.expect("."); // Deadline press was captured.
        r.quiet(150); r.pulse(150, 800);
        r.pulse(650, 150); r.pulse(150, 800);
        r.pulse(150, 150); r.pulse(150, 800);
        assert(r.pulse(650, 0)); assert(r.unlocks == 1);
    }
    Input boundary;
    boundary.pulse(650, MorseUnlock::ResetGapMs - 1); boundary.expect("- ");
    assert(boundary.recognizer.restartCount() == 0);
    boundary.quiet(1); boundary.expect("");
    assert(boundary.recognizer.restartCount() == 1);
    boundary.quiet(10000);
    assert(boundary.recognizer.restartCount() == 1); // Idle emits just one restart.
    boundary.recognizer.reset();
    assert(boundary.recognizer.restartCount() == 1);
    boundary.word(); assert(boundary.unlocks == 1);

    Input extended;
    extended.pulse(650, MorseUnlock::ResetGapMs - 1);
    extended.pulse(150, MorseUnlock::ResetGapMs - 1); extended.expect("- . ");
    assert(extended.recognizer.restartCount() == 0);
    extended.quiet(1); extended.expect("");
    assert(extended.recognizer.restartCount() == 1);

    Input held;
    held.pulse(650, MorseUnlock::ResetGapMs - 1);
    held.down();
    assert(!held.sample(10000, true, false));
    assert(held.recognizer.restartCount() == 0); held.expect("- ");
    held.quiet(0); held.quiet(MorseUnlock::ResetGapMs - 1);
    assert(held.recognizer.restartCount() == 0);
    held.quiet(1); held.expect("");
    assert(held.recognizer.restartCount() == 1);
    held.word(); assert(held.unlocks == 1);

    Input wrapped;
    wrapped.now = UINT32_MAX - 100;
    wrapped.pulse(150, MorseUnlock::ResetGapMs - 1);
    wrapped.quiet(1); wrapped.expect("");
    assert(wrapped.recognizer.restartCount() == 1);
    wrapped.word(); assert(wrapped.unlocks == 1);
}

void holdsNoiseAndAttemptLimit() {
    Input noise;
    noise.pulse(MorseUnlock::MinHoldMs - 1, 150); noise.expect("");
    noise.word(); assert(noise.unlocks == 0); noise.expect(".. -. .. - ");
    noise.quiet(MorseUnlock::ResetGapMs); noise.word(); assert(noise.unlocks == 1);

    Input tooClose;
    tooClose.pulse(150, MorseUnlock::MinSymbolGapMs - 1);
    tooClose.pulse(150, 800); tooClose.expect(".. ");
    tooClose.word(); assert(tooClose.unlocks == 0);

    Input lateRelease;
    lateRelease.pulse(MorseUnlock::MaxHoldMs + 1, 800); lateRelease.expect("");
    lateRelease.word(); assert(lateRelease.unlocks == 0);

    Input totalLimit;
    // Continue valid-sized physical taps beyond the whole-attempt limit, without
    // an idle timeout. Keep showing symbols and start the retry from last release.
    for (unsigned i = 0; i != 11; ++i) totalLimit.pulse(1400, 100);
    totalLimit.expect("-----------");
    assert(totalLimit.now > MorseUnlock::MaxAttemptMs);
    assert(totalLimit.recognizer.restartCount() == 0);
    totalLimit.pulse(150, 600); totalLimit.expect("-----------. ");
    totalLimit.quiet(MorseUnlock::ResetGapMs); totalLimit.word();
    assert(totalLimit.unlocks == 1);
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

    Input bothAtStart;
    assert(!bothAtStart.sample(0, true, true));
    bothAtStart.quiet(150); bothAtStart.word(); assert(bothAtStart.unlocks == 0);
    bothAtStart.quiet(MorseUnlock::ResetGapMs);
    bothAtStart.word(); assert(bothAtStart.unlocks == 1);

    Input disabled;
    disabled.firstThreeLetters();
    assert(!disabled.sample(0, false, false, false)); disabled.expect("");
    assert(disabled.recognizer.restartCount() == 0);
    disabled.pulse(650, 800); assert(disabled.unlocks == 0);
    disabled.quiet(MorseUnlock::ResetGapMs);
    disabled.word(); assert(disabled.unlocks == 1);

    Input disabledHeld;
    assert(!disabledHeld.sample(0, true, false, false));
    assert(!disabledHeld.sample(600, true, false));
    assert(!disabledHeld.sample(100, false, false)); disabledHeld.expect("");
    disabledHeld.word(); assert(disabledHeld.unlocks == 1);

    Input resetHeld;
    resetHeld.firstThreeLetters();
    resetHeld.recognizer.reset(true, false); resetHeld.expect("");
    assert(!resetHeld.sample(600, true, false));
    resetHeld.quiet(100);
    resetHeld.word(); assert(resetHeld.unlocks == 1);
    assert(resetHeld.recognizer.restartCount() == 0);

    Input directSwitch;
    directSwitch.down();
    assert(!directSwitch.sample(100, false, true));
    directSwitch.quiet(100); directSwitch.word();
    assert(directSwitch.unlocks == 0);
}

void exhaustiveTimingBoundaries() {
    // Every millisecond around the hold range, not just selected examples.
    for (uint32_t hold = 0; hold <= MorseUnlock::MaxHoldMs + 2; ++hold) {
        Input r;
        r.firstThreeLetters();
        const bool result = r.pulse(hold, 0);
        assert(result == (hold >= MorseUnlock::DashThresholdMs &&
                          hold <= MorseUnlock::MaxHoldMs));
        assert(r.unlocks == unsigned(result));
    }
    // Sweep both kinds of pause, including too-close, letter, and retry edges.
    for (uint32_t gap = 0; gap <= MorseUnlock::ResetGapMs + 2; ++gap) {
        Input inner;
        inner.pulse(150, gap); inner.pulse(150, 800);
        inner.pulse(650, 150); inner.pulse(150, 800);
        inner.pulse(150, 150); inner.pulse(150, 800);
        inner.pulse(650, 0);
        assert(inner.unlocks == unsigned(gap >= MorseUnlock::MinSymbolGapMs &&
                                         gap < MorseUnlock::LetterGapMs));

        Input letter;
        letter.pulse(150, 150); letter.pulse(150, gap);
        letter.pulse(650, 150); letter.pulse(150, 800);
        letter.pulse(150, 150); letter.pulse(150, 800);
        letter.pulse(650, 0);
        assert(letter.unlocks == unsigned(gap >= MorseUnlock::LetterGapMs &&
                                          gap < MorseUnlock::ResetGapMs));
    }
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
                const bool result = r.pulse(hold, gap);
                if (result) assert(i == 6); // Success comes on the seventh release.
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
    eagerFinalDashAndExactEarlierLetters();
    visibleRegisteredSymbols();
    idleTimeoutAndImmediateRetry();
    holdsNoiseAndAttemptLimit();
    ownershipAndCancellation();
    exhaustiveTimingBoundaries();
    exhaustiveSevenSymbolWords();
    std::puts("Morse unlock: eager init, grouped symbols, 2.5-second retry, bounds, cancellation and wraparound passed");
}
