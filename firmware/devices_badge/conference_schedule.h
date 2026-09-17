#pragma once
#include <stddef.h>
#include <stdint.h>

struct ConferenceScheduleItem {
  const char *title;
  const char *detail;
  int64_t startUtc;
  int64_t endUtc;
};

// Replace these rows only after actual event details are supplied. Times are
// absolute UTC Unix seconds. Both zero means deliberately untimed, not midnight.
static constexpr ConferenceScheduleItem CONFERENCE_AGENDA[] = {
  {"Session placeholder 1", "Time and details to be announced", 0, 0},
  {"Session placeholder 2", "Time and details to be announced", 0, 0},
  {"Session placeholder 3", "Time and details to be announced", 0, 0},
  {"Session placeholder 4", "Time and details to be announced", 0, 0},
  {"Session placeholder 5", "Time and details to be announced", 0, 0},
  {"Session placeholder 6", "Time and details to be announced", 0, 0},
};
static constexpr size_t CONFERENCE_AGENDA_COUNT = sizeof(CONFERENCE_AGENDA) / sizeof(CONFERENCE_AGENDA[0]);

inline int conferenceCurrentScheduleItem(const ConferenceScheduleItem *items, size_t count, int64_t nowUtc, bool clockValid) {
  if (!clockValid || nowUtc <= 0) return -1;
  // Half-open intervals: start is included, end is excluded. If supplied event
  // data overlaps, the first matching row wins; phone display offset is unused.
  for (size_t i = 0; i < count; ++i)
    if (items[i].startUtc > 0 && items[i].endUtc > items[i].startUtc && nowUtc >= items[i].startUtc && nowUtc < items[i].endUtc)
      return int(i);
  return -1;
}
