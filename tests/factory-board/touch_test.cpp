#include "board_touch.h"
#include <cassert>
#include <iostream>

int main() {
    board::TouchSample sample;
    board::acceptInjectedSample(sample, 110, 320, 357, 145, true, 10);
    assert(sample.pressed && !sample.sensor && sample.valid);
    board::acceptInjectedSample(sample, 110, 320, 357, 145, false, 20);
    assert(!sample.pressed && !sample.sensor && sample.valid);
    // Physical controller reads keep arriving while idle. Their stale/raw zero
    // values must not relabel or replace the retained simulated coordinate.
    for (uint32_t now = 30; now < 1000; now += 10) {
        board::acceptPhysicalSample(sample, true, false, 0, 0, now);
        assert(sample.valid && !sample.pressed && !sample.sensor);
        assert(sample.rawX == 110 && sample.rawY == 320);
        assert(sample.x == 357 && sample.y == 145);
    }
    board::acceptPhysicalSample(sample, false, false, 0, 0, 1000);
    assert(!sample.valid && !sample.pressed && !sample.sensor);
    assert(sample.rawX == 110 && sample.rawY == 320);
    // Only a fresh real contact owns a new physical coordinate.
    board::acceptPhysicalSample(sample, true, true, 234, 232, 1010);
    assert(sample.valid && sample.pressed && sample.sensor);
    assert(sample.rawX == 234 && sample.rawY == 232);
    board::acceptPhysicalSample(sample, true, false, 0, 0, 1020);
    assert(sample.valid && !sample.pressed && sample.sensor);
    assert(sample.rawX == 234 && sample.rawY == 232);
    std::cout << "Retained touch provenance survives idle polls until a fresh physical point\n";
}
