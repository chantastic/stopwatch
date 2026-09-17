# Offline conference badge

Factory-stack migration, September 17, 2026. Active entrypoint is
`firmware/factory_badge/main/main.cpp`; see [factory-stack.md](factory-stack.md)
for the current modules and migration verification. The behavior below remains
the product contract. Arduino-specific rendering/input details and the dated
verification reports describe the prior implementation, not the migrated build.
The prior connected AuthKit/voice application is retained in
`legacy_connected_app.h` and Git history. It is not initialized or linked into
the conference application. No gateway changes are required.

## Pages and controls

The primary pages wrap in this order, with After Dark hidden until unlocked:

1. **init()**: lightweight looping wordmark/orbit preview, local clock, battery.
2. **Schedule**: vertically scrollable published init() agenda, repeating daily
   in the badge's local time. The current block says **On now**; passed blocks
   are dimmed. Times, wrapped titles and available speaker details stay visible.
3. **Developers After Dark**: honest invite placeholder with a reserved QR area.
   Hidden initially; unlock with Morse `init` or the 1:30 PM reveal below.
   It deliberately has no scannable event destination yet.
4. **Badge**: manual name/photo, init() brand, selected network icon/label and QR.
   Swipe vertically through GitHub, X/Twitter and LinkedIn, including empty slots.
5. **Hack your Badge**: real QR to `https://drop.workos.cloud/stopwatch`.
6. **Settings**: battery percentage, brightness, local date/time, phone setup,
   touch test, and orientation. All controls fit on one page; no settings
   scrolling is needed.

The factory 468×466 framebuffer retains static side chevrons, top clock, bottom page
name, and five centered dots before unlock, six afterward. Useful content and QR quiet zones remain inside the round
aperture. Name display truncates at UTF-8 boundaries; the complete accepted value
remains editable in setup. Standard firmware fonts have limited glyph coverage.

Screen-left/right pushers page backward/forward at rotations 0 and 2. In the
lanyard-up rotation 2, **blue is previous and yellow is next**. In rotation 0,
yellow is previous and blue next. At quarter turns, the pushers are top/bottom;
use the stock yellow-previous/blue-next convention. Side arrows also accept a
completed tap, and a horizontal swipe pages. Both pushers open setup from the
primary pages. Either pusher or the on-screen Back/cancel button closes setup.
Inside Touch test, either pusher or both together instead return to Settings.

Schedule drag scrolling is clipped above the footer; the last row is reachable.
Badge vertical swipes change only the network slot. The selected network is saved
after a short debounce; primary pages start on init() at reboot. Empty accounts
never inherit another slot's QR. Tap an empty badge to configure; tap a configured
badge to expand/shrink its QR. Both pushers can edit any configured profile.

Gestures dispatch on release. A drag that returns to its start is still not a
tap. Long holds do not activate taps. Setup is modal: drags cannot change the
hidden page, network, or schedule position. Rotation uses the established IMU
axis mapping/filter and stays stable throughout touch.

## Secret After Dark reveal

Use **either physical pusher for the entire word** to enter `init` in Morse:
`.. / -. / .. / -` (two taps; hold then tap; two taps; hold). Navigation keeps
working while the badge listens on all primary pages. Use short taps around
150 ms, holds around 600 ms, short pauses within letters, and roughly one second
between letters. Wait for the pause after the final hold; the invitation opens
automatically. After an incorrect attempt, leave both buttons alone for three
seconds before retrying. Switching pushers mid-word or pressing both cancels.

The recognizer accepts dots of 50–349 ms and dashes of 350–1400 ms. Symbol gaps
are 50–599 ms; letter pauses are 600–2999 ms; a whole attempt must finish within
15 seconds. Setup and Touch test discard progress, and their exit press cannot
start a new code. These are monotonic timers, unaffected by phone/USB clock sync.
USB `button` actions do not represent hold durations and cannot enter Morse.

The automatic reveal has a fixed date/time cutoff: **October 7, 2026 at 1:30 PM
in the badge's configured local time**. Any valid clock reading at or after that
instant unlocks it, including a fresh badge first started the next morning,
midnight, or a later date. Earlier dates never trigger it, even after 1:30 PM.
The agenda still repeats daily; this reveal does not. An unset clock keeps it
hidden, but Morse still works.
The timed reveal adds the page and its dot without changing the current view,
scroll position, or modal. Settings remains the last page. Stable internal IDs
remain 0–5; navigation skips ID 2 until revealed.

Either reveal latches in the separate versioned `conference_ui/after_dark_v1`
NVS byte. It is saved promptly; failures keep the page open and retry after five
seconds. A committed unlock survives restart, midnight, and clock corrections.
Ordinary reflashing preserves it. A loss of power before a successful save can
lose a Morse unlock. This is an Easter egg, not a security boundary.

The policy lives in `after_dark_unlock.h`, recognition in `morse_unlock.h`, and
the main task connects those helpers to existing debounced board input, clock,
NVS and the UI model. Views never read the clock or storage directly.

## Settings and current schedule item

Brightness applies immediately in ten-percentage-point steps, bounded to 10–100%
with a 50% default. The minimum maps to a nonzero display level. Brightness and
orientation share one versioned NVS value in `conference_ui`; writes coalesce
after 1.2 seconds without another change. The footer says `Saving settings...`
while pending, and a failed write stays pending with a five-second retry.
The existing selected-network preference remains separate. Changes made just
before power loss may not have reached the delayed save yet.

Orientation has exactly three choices: **Free** resumes calibrated automatic
rotation in all four directions; **Default** fixes the stock rotation 0;
**180°** fixes rotation 2, the lanyard-up pose. These fixed choices were swapped
after the user's September 17 device feedback. Default is a named fixed pose, not the orientation
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

The active agenda is `firmware/factory_badge/main/schedule.h`, sourced from
[workos.com/init](https://workos.com/init) on September 17, 2026. Its nine blocks
start at 8:00 AM, 9:30 AM, 11:00 AM, 11:30 AM, 12:30 PM, 1:30 PM, 3:00 PM,
3:30 PM and 5:00 PM. Only the keynote currently has an assigned speaker;
unannounced program speakers stay TBA rather than being inferred from the
separate speaker list.

At the user's request, the agenda intentionally repeats **every day** instead
of being restricted to October 7. Current local minute is calculated from UTC
plus the same saved offset as the badge clock. Starts are inclusive; the next
start ends each block. The final Happy hour block stays current until midnight
because no end time is published; its detail says `End time not listed`.
Before 8:00 AM every block is upcoming. Midnight resets all rows to upcoming.
An invalid clock selects no current/passed rows.

On entry, the schedule scrolls to the current block. Subsequent clock changes
update the highlight and dimming in place without moving the user's reading
position. Titles and details wrap; row heights are based on their content.
The older absolute-UTC helper in `firmware/devices_badge/conference_schedule.h`
is retained only for the Arduino application and its historical tests.

## Touch alignment test

Open **Settings → Touch test** to compare five white crosshair targets with the
live purple position reported by the touch sensor. Touch or drag over the top,
center, bottom, left and right targets. The final marker and coordinate readout
remain visible after release so the finger does not obscure the result.

The current display rotation is held for the duration of the test without
changing the saved orientation mode. Either physical pusher, or both together,
returns to Settings. Choose **Default** or **180°**, let the display rotate, then
reopen the test to compare those poses. The test does not calibrate the sensor,
save touch data, or start Wi-Fi or Bluetooth.

The live marker uses the factory CST820 sample and the same LVGL rotation as
normal input. The retired Arduino scale/offset fit is not applied. See the
[hardware notes](hardware.md#combined-offset-trial) for that historical trial.
The test does not create or save calibration.
Serial `touch` input exercises the test display but is labeled **Simulated input**.
It verifies dispatch and rendering, not sensor alignment. Physical alignment
and any remaining offset still require the user's observation on the device.
`touch_test_status` returns active/pressed/sample flags, sensor-versus-simulated
source, corrected screen/raw coordinates, held rotation, `scale_trial`,
`touch_model` (`factory-native`, with `scale_trial:false`), and an optional valid hex nonce.
It reads only the temporary test state and does not activate the test.

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
schedule index. `page_count` is the visible count; `after_dark_unlocked` and
`after_dark_save_pending` report reveal/persistence state without user data.
A write count is per boot and is not a flash-wear measurement.

`scripts/verify-factory.py` captures the currently visible pages. It checks the
fresh reveal state before visiting After Dark and records a locked invitation in
`skipped_pages`, alongside the stable IDs in `captured_pages`. It never changes
the clock or unlocks the invitation to increase capture coverage. A skipped page
is not a verification of its invitation UI.

Run `scripts/test.sh` and `scripts/build.sh`. Native checks cover real LVGL
rotation, UI input/rendering, RTC validation, and cross-version profile storage.
Retained Arduino host checks cover gestures,
scroll bounds, network isolation, URL/name/image limits, atomic-storage failures,
portal commit/cancel/timeout behavior, and fresh RTC/batch acknowledgments.
Settings checks cover brightness bounds/debounce, fixed modes, drag rejection,
and schedule UTC boundaries/corrections. The actual rendered portal script is
executed in host fixtures to verify fresh clock requests, offset signs and retry.
The older host suites remain regression checks for retained connected code.
Hardware verification results and limits belong in the dated report below;
compilation/host simulations alone are not proof of physical behavior.

## Verification record

Secret invitation behavior and its host/device limits are recorded in the
[After Dark verification report](after-dark-verification-2026-09-17.md).

Current native ESP-IDF/LVGL results are in the
[factory verification report](factory-verification-2026-09-17.md).
The populated daily agenda is covered by the
[schedule verification report](schedule-verification-2026-09-17.md).
The reports below describe the retired Arduino implementation and are retained
as historical evidence, not acceptance of the new input or network code.

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
versions and MIT licensing, and [factory stack](factory-stack.md) for the later
decision to migrate the runtime, board adapter and views to the factory stack.
