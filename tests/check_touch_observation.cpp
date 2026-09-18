#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../firmware/factory_badge/main/touch_observation.h"

using Observation = badge_touch::Observation;
using Contact = Observation::Contact;
using End = Contact::End;

namespace {
void sample(Observation& o, uint32_t now, bool down, int x = 200, int y = 240) {
    o.observe(true, true, down, true, x, y, now);
}
Contact next(Observation& o) {
    Contact c;
    assert(o.pop(c));
    return c;
}

void timingAndMovement() {
    Observation o;
    sample(o, 10, true);
    sample(o, 110, false);
    assert(!o.active(110) && o.count() == 0);
    o.start(200);
    sample(o, 200, false);
    sample(o, 250, true);
    sample(o, 290, true, 223, 221);
    sample(o, 320, true); // Moving back cannot hide the previous excursion.
    sample(o, 400, false);
    sample(o, 570, true, 250, 260);
    sample(o, 1220, false, 250, 260);
    Contact c = next(o);
    assert(c.press_ms == 250 && c.release_ms == 400 && c.duration_ms == 150);
    assert(!c.has_previous && c.gap_before_ms == 0);
    assert(c.start_x == 200 && c.start_y == 240 && c.end_x == 200 && c.end_y == 240);
    assert(c.max_dx == 23 && c.max_dy == 19 && c.end == End::Released);
    assert(std::strcmp(c.reason(), "release") == 0);
    c = next(o);
    assert(c.duration_ms == 650 && c.has_previous && c.gap_before_ms == 170);
    assert(o.count() == 0 && o.dropped() == 0);
    assert(!o.pop(c));
}

void heldStartAndScopeEntry() {
    Observation o;
    o.start(100);
    sample(o, 90, false); // A cached pre-session sample cannot arm or expire it.
    sample(o, 120, true);
    sample(o, 220, false);
    assert(o.active(220) && o.count() == 0);
    sample(o, 300, true);
    o.observe(false, true, true, true, 201, 241, 350);
    Contact c = next(o);
    assert(c.end == End::Ineligible && c.duration_ms == 50);
    // Leaving/reentering the page while held never captures the contact tail.
    sample(o, 400, true);
    sample(o, 500, false);
    assert(o.count() == 0);
    sample(o, 600, true);
    sample(o, 700, false);
    c = next(o);
    assert(c.end == End::Released && !c.has_previous);
    // A first held sample after entering a new scope is ignored too.
    o.observe(false, true, false, true, 200, 240, 800);
    o.observe(false, true, true, true, 200, 240, 900);
    sample(o, 1000, true);
    sample(o, 1100, false);
    assert(o.count() == 0);
}

void lossAndInjection() {
    Observation o;
    o.start(0);
    sample(o, 0, false);
    sample(o, 100, true);
    o.observe(true, false, false, true, 0, 0, 150);
    Contact c = next(o);
    assert(c.end == End::Lost && c.duration_ms == 50);
    assert(c.end_x == 200 && c.end_y == 240); // Failure is not a coordinate.
    sample(o, 200, true);
    sample(o, 250, false);
    assert(o.count() == 0);
    o.observe(true, true, true, false, 400, 400, 300);
    o.observe(true, true, false, false, 400, 400, 350);
    assert(o.count() == 0); // Injected presses/releases cannot become evidence.
    sample(o, 400, false);
    sample(o, 500, true);
    o.observe(true, true, false, false, 400, 400, 600);
    c = next(o);
    assert(c.end == End::Lost && c.duration_ms == 100 && !c.has_previous);
    assert(c.end_x == 200 && c.end_y == 240);
    sample(o, 650, true);
    sample(o, 700, false);
    assert(o.count() == 0);
    sample(o, 800, true);
    sample(o, 900, false);
    c = next(o);
    assert(c.end == End::Released && !c.has_previous);
}

void boundedSessions() {
    Observation o;
    o.start(100, 1000);
    sample(o, 100, false);
    sample(o, 500, true);
    assert(o.active(1099));
    assert(!o.active(1200));
    Contact c = next(o);
    assert(c.end == End::Expired && c.release_ms == 1100 && c.duration_ms == 600);
    sample(o, 1250, false);
    assert(o.count() == 0);
    o.start(2000, UINT32_MAX);
    assert(o.active(121999));
    assert(!o.active(122000)); // Duration is always capped at two minutes.
    o.start(130000, 0);
    assert(!o.active(130000));
    o.start(140000);
    sample(o, 140000, false);
    sample(o, 140100, true);
    o.stop(140200);
    c = next(o);
    assert(c.end == End::Stopped && c.duration_ms == 100 && !o.active(140200));
    o.stop(140300);
    assert(o.count() == 0);
    o.start(150000, 1000);
    sample(o, 150000, false);
    sample(o, 150100, true);
    sample(o, 151010, false); // A late lift cannot replace timeout with release.
    c = next(o);
    assert(c.end == End::Expired && c.release_ms == 151000);
    o.start(160000, 1000);
    sample(o, 160000, false);
    sample(o, 160100, true);
    o.stop(161200);
    assert(next(o).end == End::Expired);
}

void fifoOverflowAndRestart() {
    Observation o;
    o.start(0);
    sample(o, 0, false);
    for (uint32_t i = 0; i < 70; ++i) {
        sample(o, 100 + i * 200, true);
        sample(o, 200 + i * 200, false);
    }
    assert(o.count() == Observation::Capacity && o.dropped() == 6);
    for (uint32_t i = 6; i < 70; ++i) {
        const Contact c = next(o);
        assert(c.press_ms == 100 + i * 200 && c.duration_ms == 100);
        assert(c.has_previous && c.gap_before_ms == 100);
    }
    assert(o.count() == 0 && o.dropped() == 6);
    sample(o, 14100, true);
    sample(o, 14200, false);
    assert(next(o).press_ms == 14100);
    o.start(15000);
    assert(o.dropped() == 0 && o.count() == 0);
    sample(o, 15100, true);
    sample(o, 15200, false);
    assert(o.count() == 0); // A fresh session waits for release again.
}

void wraparound() {
    Observation o;
    constexpr uint32_t begin = UINT32_MAX - 200;
    o.start(begin, 1000);
    sample(o, begin, false);
    sample(o, begin + 50, true);
    sample(o, begin + 250, false);
    sample(o, begin + 400, true);
    sample(o, begin + 700, false);
    Contact c = next(o);
    assert(c.duration_ms == 200 && c.release_ms == 49);
    c = next(o);
    assert(c.duration_ms == 300 && c.has_previous && c.gap_before_ms == 150);
    sample(o, begin + 800, true);
    assert(!o.active(begin + 1000));
    c = next(o);
    assert(c.end == End::Expired && c.duration_ms == 200 && c.release_ms == 799);
}
}  // namespace

int main() {
    timingAndMovement();
    heldStartAndScopeEntry();
    lossAndInjection();
    boundedSessions();
    fifoOverflowAndRestart();
    wraparound();
    std::puts("Touch observation checks passed");
}
