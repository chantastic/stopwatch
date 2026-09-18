#include "../firmware/factory_badge/main/schedule_bookmarks.h"
#include <cassert>
#include <cstdio>
#include <cstdint>

int main() {
    using badge_schedule::Bookmarks;
    Bookmarks marks;
    assert(marks.mask() == 0 && !marks.pending());
    assert(!marks.restore(0));
    assert(!marks.restore(Bookmarks::Version | (uint32_t(1) << badge_schedule::Items.size())));
    assert(!marks.restore(0xb5020000u));
    assert(marks.restore(Bookmarks::Version | 0x101u));
    assert(marks.bookmarked(0) && marks.bookmarked(8) && !marks.bookmarked(1));
    assert(!marks.bookmarked(-1) && !marks.bookmarked(9));
    assert(!marks.pending());
    assert(!marks.toggle(-1, 0) && !marks.toggle(9, 0) && !marks.pending());
    assert(marks.toggle(1, 100));
    assert(marks.encoded() == (Bookmarks::Version | 0x103u));
    assert(!marks.saveDue(1299) && marks.saveDue(1300));
    // Rapid taps restart the debounce and preserve all other choices.
    assert(marks.toggle(8, 1200));
    assert(marks.mask() == 3 && !marks.saveDue(2399) && marks.saveDue(2400));
    marks.saveFailed(2400);
    assert(marks.pending() && !marks.saveDue(7399) && marks.saveDue(7400));
    // A new choice after a failed write uses the normal debounce again.
    assert(marks.toggle(0, 3000));
    assert(marks.mask() == 2 && !marks.saveDue(4199) && marks.saveDue(4200));
    Bookmarks restored;
    assert(restored.restore(marks.encoded()) && restored.mask() == 2 && !restored.pending());
    marks.saved();
    assert(!marks.pending() && !marks.saveDue(UINT32_MAX));
    // Monotonic millisecond rollover does not lose a pending write.
    assert(marks.toggle(2, UINT32_MAX - 500));
    assert(!marks.saveDue(698) && marks.saveDue(699));
    marks.saveFailed(UINT32_MAX - 1000);
    assert(!marks.saveDue(3998) && marks.saveDue(3999));
    marks.saved();
    // Restoring malformed data clears stale memory without scheduling writes.
    assert(!marks.restore(Bookmarks::Version | 0x8000u));
    assert(marks.mask() == 0 && !marks.pending());
    std::puts("schedule bookmark persistence policy passed");
}
