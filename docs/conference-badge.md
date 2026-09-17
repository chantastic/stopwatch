# Offline conference badge

Current Settings release, September 17, 2026. Active entrypoint is
`firmware/devices_badge/devices_badge.ino`, which includes `conference_app.h`.
The prior connected AuthKit/voice application is retained in
`legacy_connected_app.h` and Git history. It is not initialized or linked into
the conference application. No gateway changes are required.

## Pages and controls

The six primary pages wrap in this fixed order:

1. **init()**: lightweight looping wordmark/orbit preview, local clock, battery.
2. **Schedule**: vertically scrollable, explicitly labeled placeholder rows.
   No event times, speakers, venues, or actual agenda have been supplied.
3. **Developers After Dark**: honest invite placeholder with a reserved QR area.
   It deliberately has no scannable event destination yet.
4. **Badge**: manual name/photo, init() brand, selected network icon/label and QR.
   Swipe vertically through GitHub, X/Twitter and LinkedIn, including empty slots.
5. **Hack your Badge**: real QR to `https://drop.workos.cloud/stopwatch`.
6. **Settings**: battery percentage, brightness, local date/time, phone setup,
   and orientation. All controls fit on one page; no settings scrolling is needed.

The 468×468 framebuffer retains static side chevrons, top clock, bottom page
name, and six dots. Useful content and QR quiet zones remain inside the round
aperture. Name display truncates at UTF-8 boundaries; the complete accepted value
remains editable in setup. Standard firmware fonts have limited glyph coverage.

Screen-left/right pushers page backward/forward at rotations 0 and 2. In the
lanyard-up rotation 2, **blue is previous and yellow is next**. In rotation 0,
yellow is previous and blue next. At quarter turns, the pushers are top/bottom;
use the stock yellow-previous/blue-next convention. Side arrows also accept a
completed tap, and a horizontal swipe pages. Both pushers open setup. Either
pusher or the on-screen Back/cancel button closes setup.

Schedule drag scrolling is clipped above the footer; the last row is reachable.
Badge vertical swipes change only the network slot. The selected network is saved
after a short debounce; primary pages start on init() at reboot. Empty accounts
never inherit another slot's QR. Tap an empty badge to configure; tap a configured
badge to expand/shrink its QR. Both pushers can edit any configured profile.

Gestures dispatch on release. A drag that returns to its start is still not a
tap. Long holds do not activate taps. Setup is modal: drags cannot change the
hidden page, network, or schedule position. Rotation uses the established IMU
axis mapping/filter and stays stable throughout touch.

## Settings and current schedule item

Brightness applies immediately in ten-percentage-point steps, bounded to 10–100%
with a 50% default. The minimum maps to a nonzero display level. Brightness and
orientation share one versioned NVS value in `conference_ui`; writes coalesce
after 1.2 seconds without another change. The footer says `Saving settings...`
while pending, and a failed write stays pending with a five-second retry.
The existing selected-network preference remains separate. Changes made just
before power loss may not have reached the delayed save yet.

Orientation has exactly three choices: **Free** resumes calibrated automatic
rotation in all four directions; **Default** fixes rotation 2, the normal lanyard
pose; **180°** fixes rotation 0. Default is a named fixed pose, not the orientation
at the moment of selection. The selected mode survives restart. Startup in Free
begins at rotation 2 until fresh IMU readings settle. Switching modes clears old
filter candidates and waits until navigation and physical touch are fully
released before rotating. Pusher direction follows the displayed orientation.

Battery is the percentage reported by the board driver, refreshed on Settings;
an unavailable reading is labeled honestly. No remaining runtime or charging
claim is inferred. Date/time uses the shared clock with its saved display offset,
or `Date / time not set` when invalid. **Connect phone** opens the same temporary
local setup used from Badge. Setup launched from Settings returns to Settings
after Save, Cancel or timeout; successful setup launched elsewhere retains the
prior return to Badge, and cancellation leaves its launch page selected.

Schedule entries can carry explicit UTC start/end timestamps in
`conference_schedule.h`. A valid clock highlights the first matching interval
where `start <= now < end`, with an accent border and `Now /` label. Gaps, invalid
intervals, invalid clocks and untimed rows have no current selection. Display
timezone offsets never alter those UTC comparisons. Forward/backward clock
corrections re-evaluate selection without moving the user's scroll position.
The shipped placeholder rows remain untimed; no real agenda has been invented.

## Local customization

Setup creates a temporary password-protected hotspot with a per-device SSID.
Scan its Wi-Fi QR, then use the captive page or `http://192.168.4.1`.
There is no internet requirement, station connection, scraping, or sign-in.
On each page load, the browser automatically submits its current epoch and UTC
offset to a separate authorized clock endpoint. Clock status and Retry are
independent of Save badge. Each retry samples time again. A successful sync is
kept even if the profile edits are cancelled; failed sync shows an error and
does not silently save edits or close setup. See [the clock guide](conference-clock.md).
The committed name and all three canonical profile URLs prefill the form.
GitHub, X/Twitter and LinkedIn personal profile handles/HTTPS URLs are accepted;
unrelated hosts, extra paths, control characters, and excessive input are rejected.

Choose a JPEG, PNG, or WebP photo in the browser. Browser JavaScript converts it
locally to JPEG at most 512×512 and 128 KiB. The device independently checks the
JPEG size/dimensions, decodes in PSRAM, and stores a 160×160 center crop. Keep,
replace, and remove are explicit choices. An image upload is only staged until
Save badge commits all fields and the image together.

The phone receives a save/cancel acknowledgment before a three-second grace
period closes the AP. Connection loss during save is an uncertain acknowledgment:
check the badge or reopen setup to see the committed values. Cancel and timeout
discard staged changes. Setup also expires after ten minutes. Normal mode has
both radios off, including after save, cancel, timeout and reboot.

HTTP transport processes at most 2 KiB and one nonblocking socket operation per
loop tick. Headers have a five-second absolute deadline, the complete request
ten seconds, responses five seconds, and idle connections 1.5 seconds. These
bounds keep trickled uploads and stalled response readers from monopolizing the
input loop. Requests are checked before body allocation; multipart, chunked,
oversized, ambiguous, and unauthorized requests are refused. Save acknowledgment
grace begins once the response is queued completely or its connection fails.

## Storage and privacy

The existing `ffat` partition still holds LittleFS at offset `0x610000`, length
`0x9E0000`. A separate conference record stores manual fields plus RGB565 image,
with versioned bounds and SHA-256. Save writes a temporary record, syncs/closes,
reads/verifies it, then atomically renames it. RAM and form prefill update only
after that commit. A staged image is volatile. The loader refuses corrupt records.

Conference source never reads legacy authenticated profiles, Wi-Fi passwords,
sessions, or pending voice receipts. Those previous records are preserved by
ordinary flashing. Manual profiles confer no authentication. Storage remains
unencrypted; images and profile fields belong on the device, not in source,
binaries, public logs, or screenshots committed to Git.

No automatic filesystem formatting occurs on mount errors. A batch unit without
usable LittleFS within the verified expected partition map can use the explicit option documented in
[the clock and batch guide](conference-clock.md). That opt-in initializes `ffat`
and destroys previous filesystem contents in that partition; never use it as
recovery for an attendee's damaged profile. Default flashing fails readiness
instead of reporting such a unit ready.
Factory layouts that differ are blocked before upload and need a separately
authorized migration. Filesystem initialization cannot migrate a partition map.

## Diagnostics and verification

Use one serial owner at 115200 baud. `status` returns only bounded system/state
metadata, plus a supplied valid hex nonce for freshness. `page` ±1, `button`
blue/yellow/both, and `touch` begin/move/end exercise the same navigation dispatch.
They simulate input; they do not constitute physical touch/button testing.
`capture_badge` emits the existing bounded RGB frame protocol and refuses the
setup screen, which contains temporary Wi-Fi credentials. Captures are private.

`setup_test` accepts an input-only ephemeral alphanumeric AP password for
repeatable local verification; it is never saved or echoed. `clear_manual_profile`
with boolean `confirm:true` clears only the conference record. It never erases
the filesystem, NVS, or legacy authenticated records. Do not clear attendee data
as part of routine flashing. These diagnostics have the same physical USB trust
boundary as application flashing.

`portal_status` reports transport counters, phases, bounded byte counts and close
reasons, with separate last-POST evidence so captive probe GETs cannot overwrite
it. It does not export request paths, headers, bodies, nonce or profile values.
`status` includes the AP client count, without client identifiers, plus brightness,
orientation mode, pending preference state/write count, page count, and current
schedule index. A write count is per boot and is not a flash-wear measurement.

Run `scripts/test.sh` and `scripts/build.sh`. New host checks cover gestures,
scroll bounds, network isolation, URL/name/image limits, atomic-storage failures,
portal commit/cancel/timeout behavior, and fresh RTC/batch acknowledgments.
Settings checks cover brightness bounds/debounce, fixed modes, drag rejection,
and schedule UTC boundaries/corrections. The actual rendered portal script is
executed in host fixtures to verify fresh clock requests, offset signs and retry.
The older host suites remain regression checks for retained connected code.
Hardware verification results and limits belong in the dated report below;
compilation/host simulations alone are not proof of physical behavior.

## Verification record

See the [Settings verification report](conference-verification-2026-09-17.md)
and the [earlier scaffold report](conference-verification-2026-09-16.md).
Private Settings captures/results stay under `.build/settings-verification/`.
Actual Settings persistence, rendering and input dispatch pass on the development
board. Phone clock synchronization and device profile save/edit/photo persistence
remain unverified: the bounded Settings attempt failed at workstation hotspot
association, while the earlier AP/browser failures are documented separately.
Production portal/profile/clock host fixtures pass; they are not a completed
phone or hardware profile test.

See [stock UI source research](stock-ui-reference.md) for the inspected framework,
versions, MIT licensing, and the decision to reuse navigation patterns while
keeping the pinned Arduino/M5Unified drivers.
