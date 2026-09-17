# Daily agenda verification — September 17, 2026

`conference-factory-3` uses the nine published blocks from
[workos.com/init](https://workos.com/init), checked on this date. Session content
and daily timing rules live in `firmware/factory_badge/main/schedule.h`; the
native view is `main/ui/page_schedule.cpp`.

## Behavior

- Time, wrapped title and available speaker details appear in each card.
- The current block has an accent border/background and **On now** label;
  passed blocks have reduced opacity. Future blocks retain normal contrast.
- The schedule opens with the current block visible. Live clock changes update
  existing rows without changing row heights or the attendee's scroll position.
- The agenda repeats daily using the badge's saved local UTC offset. Each next
  start ends the previous block. Happy hour has no published end, so the daily
  model keeps it current from 5:00 PM to midnight and displays that the end is
  unlisted. Midnight clears all passed states; before 8:00 AM all are upcoming.
- An invalid clock shows setup guidance and no current/passed styling. Speaker
  slots marked TBA on the source remain TBA; the separate speaker list is not
  assigned to unpublished sessions.

## Verification

Host checks passed for exact starts/ends, every minute of the daily cycle,
midnight/year/date repetition, positive/negative/fractional-hour offsets, clock
corrections and invalid clocks. The actual LVGL view passed sanitized tests for
wrapped content bounds, current/past/future styling, initial focus, preserved
scroll/row heights, midnight reset and no idle redraws. Provisioning/flash helper
tests also passed.

Native build and upload passed with the existing partition map and stored state
preserved. Application size is **1,676,960 bytes**. SHA-256:
`78e3c38b36cd4a1a75fd49c707f4796b522d03bfbc28fa4ca83c7c8b966cce73`.

On the attached badge, `scripts/verify-schedule.py` checked ten representative
local times: before opening, check-in, keynote, break, lunch, afternoon, Happy
hour, 11:59 PM, midnight and the next day's keynote. The reported current index
matched each case. Frame captures were reviewed for the displayed clock,
readable content and current/passed styles.

The test restored fresh real time with the saved offset and left Schedule open.
Final observation at 1:14 PM showed Lunch and networking as On now, with the
preceding morning program dimmed. Brightness, orientation, selected network and
profile presence matched before the update; Wi-Fi and Bluetooth were off.

Evidence stays private under `.build/schedule-verification/`. These software,
RTC and framebuffer checks do not constitute a new physical touch-alignment or
battery-runtime measurement. The prior rotated-panel fix remains in place.
