#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Published agenda: https://workos.com/init, checked September 17, 2026.
// Times intentionally repeat daily in the badge's configured local timezone.
// The site lists starts only: each block runs until the next start, with the
// final block remaining current until midnight (its actual end is unpublished).
namespace badge_schedule {
struct Item {
    int minute;
    const char* time; // Displayed h:mm AM/PM; minute remains the scheduling value.
    const char* title;
    const char* detail;
};
inline constexpr std::array<Item, 9> Items{{
    {8 * 60, "8:00 AM", "Check-in and breakfast", ""},
    {9 * 60 + 30, "9:30 AM", "Opening keynote", "Michael Grinich / WorkOS"},
    {11 * 60, "11:00 AM", "Networking break and sponsors", ""},
    {11 * 60 + 30, "11:30 AM", "Morning program (continued)", "Speakers TBA"},
    {12 * 60 + 30, "12:30 PM", "Lunch and networking", ""},
    {13 * 60 + 30, "1:30 PM", "Afternoon program", "Speakers TBA"},
    {15 * 60, "3:00 PM", "Networking break and sponsors", ""},
    {15 * 60 + 30, "3:30 PM", "Afternoon program (continued)", "Speakers TBA"},
    {17 * 60, "5:00 PM", "Happy hour", "End time not listed"},
}};

enum class State { Unknown, Upcoming, OnNow, Passed };

inline int localMinute(int64_t utc, int offsetMinutes, bool valid) {
    if (!valid || utc <= 0 || offsetMinutes < -840 || offsetMinutes > 840) return -1;
    // Reduce before adding the offset so even large valid integers cannot
    // overflow. No calendar date is involved in this intentionally daily view.
    return int((utc % 86400 + int64_t(offsetMinutes) * 60 + 86400) % 86400 / 60);
}

inline int endMinute(size_t index) {
    return index + 1 < Items.size() ? Items[index + 1].minute : 1440;
}

inline State state(size_t index, int minute) {
    if (index >= Items.size() || minute < 0 || minute >= 1440) return State::Unknown;
    if (minute < Items[index].minute) return State::Upcoming;
    return minute < endMinute(index) ? State::OnNow : State::Passed;
}

inline int current(int minute) {
    for (size_t i = 0; i < Items.size(); ++i)
        if (state(i, minute) == State::OnNow) return int(i);
    return -1;
}
} // namespace badge_schedule
