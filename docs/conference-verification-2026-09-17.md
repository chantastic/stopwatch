# Conference Settings verification — September 17, 2026

## Physical acceptance follow-up

The user tested the installed scale correction (`213a527`) in Default orientation.
A subsequent one-minute live observation captured 54 completed sensor taps;
median distance to the inferred target improved from 31.1 to 10.0 pixels, and
bottom vertical median error improved from +39 to 0 pixels. This supersedes the
earlier lack of fresh physical observations below. See
[hardware notes](hardware.md#physical-follow-up-with-the-scale-correction) for
per-target results, evidence locations, and the remaining 180°/fleet limitations.
The observation changed no firmware or settings and ended with the serial port
closed. The user reported that the correction felt good; retain it for now.

## Follow-up: measured continuous touch scaling

The user requested a continuous scale correction based on the earlier physical
test, with the upper region remaining close. The development-board trial now
uses the per-axis scale/offset fit documented in
[hardware notes](hardware.md#continuous-scale-trial). It does not use per-target
lookup, snapping, an assumed exact 1.2 factor, or saved calibration parameters.

Full host tests and the firmware build pass. Regression checks cover the five
recorded region medians, monotonic scaling, unclamped out-of-range coordinates,
and thousands of points across all four orientations (within one pixel of the
rotate/correct/rotate reference). Independent review reproduced the fitted
coefficients and checked both physical input call sites receive the correction
exactly once, while injected display coordinates remain unchanged.

The guarded uploader verified the existing partition map, component writes,
and fresh RTC/storage/radio readiness, with no filesystem initialization.
On-board dispatch checks confirmed the new `scale_trial` flag, touch-test entry,
unchanged synthetic coordinates, pusher exit, and reopening with no old marker.
Starting preferences/profile metadata were preserved. A subsequent 60-second
live observation received no physical touches, so this trial has **not** yet
been physically validated. The test was left open in its entry rotation 2,
with saved mode Free and brightness 50%; the serial observer closed afterward.

Application SHA-256:
`f0bfc8bd6619985a53d93500a10ddcf45ca6224dcf041a40221b3e950baf7073`.
Private evidence: `.build/touch-scale-{tests,build,flash}.log` and
`.build/touch-test-verification/scaled-live-20260917-091932.jsonl`.

## Follow-up: Settings touch test

Settings now includes **Touch test** next to Connect phone. The modal shows five
targets, a live marker from cached raw sensor input transformed once by M5GFX,
and the last sample after release. It holds the entry rotation without changing
the saved mode. Either pusher or the chord returns to Settings; any remaining
physical contact is ignored until full release. It applies no calibration.

The complete host suite, firmware build and independent code review pass.
Ordinary guarded upload verified the existing partition map and application
writes, then reported fresh RTC `UNIT_READY`, with no storage initialization.
Actual-firmware USB dispatch checks passed for opening-tap exclusion, tiny
sample movement, long drag isolation, retained release position, and blue,
yellow and chord exits in Default/180°/Free respectively. Test activity did not
write preferences, change profiles or open networking. The relocated Connect
phone control still opens setup and returns to Settings with radios off.
Framebuffer captures were visually checked within the circular display.

These are simulated input checks on the board. Real sensor alignment, physical
button timing and moved-board behavior remain for hands-on observation. The
badge was left on Settings, with its starting Free mode and 50% brightness.
Application SHA-256:
`f5c56219da104d5db002deb826453a44a014ed0bf083cae8dd085d26282f3eb3`.
Private captures/results are under `.build/touch-test-verification/`; host,
build and upload logs are `.build/touch-test-{host,build,flash}.log`.

## Follow-up: fixed orientation labels corrected

After device feedback on the initial `2dcd246` release, the fixed choices now
use **Default → rotation 0** and **180° → rotation 2**. The earlier observations
below describe the initial release's opposite mapping. Free calibration,
touch-release gating, physical pager mapping and saved preference encoding are
unchanged; saved fixed mode names now select their corrected poses.

The complete host suite and firmware build pass. The ordinary guarded upload
matched the existing partition map, verified writes, and returned fresh RTC
`UNIT_READY` without initializing storage. Actual-firmware USB dispatch checks
confirmed both corrected fixed rotations, selection only after release, physical
pager mapping by displayed rotation, and both modes surviving restart. These are
diagnostic inputs, not a physical touch or moved-board test. The initial Free
mode, 50% brightness and init() page were restored; profile metadata and network
selection remained unchanged, with both radios off.

The corrected application SHA-256 is
`fc187bc953a28b265d37707688a59465e8ae5bfcd46f50350d2ced48992bdf06`.
Private evidence: `.build/orientation-correction-verification.json`,
`.build/orientation-tests.log`, `.build/orientation-build.log`, and
`.build/orientation-flash.log`. Phone acceptance limits below remain unchanged.

## Initial Settings release

Primary checkout: `/Users/chan/Developer/m5stack-conference-badge`, branch
`codex/offline-conference-badge`, following scaffold commit `d1ac792`.
Runtime build: `conference-settings-2`. This report covers one development
StopWatch, not factory migration or a production batch qualification.

Final compiled code: 1,304,051 bytes; globals: 54,708 bytes. Application artifact:
1,304,192 bytes, SHA-256
`d5634126e6764f93a4fc53ccb1c2d1dabf1102b81978cb30da4098ae5fcff828`.
The private release manifest binds source and component hashes under
`.build/settings-verification/release.json`.

## Actual device checks

- The ordinary guarded uploader matched the current partition sector to the
  compiled map and followed the same USB identity across resets. Application
  components were verified after upload. No chip erase, filesystem initialization,
  partition migration, profile clear, or NVS/filesystem image was performed.
- Fresh computer time was written to the hardware RTC, read back and checked
  for advancement. The final `UNIT_READY` reported the expected board/memory,
  mounted storage, valid clock and both radios off. The empty manual profile
  remained empty throughout testing.
- Settings is sixth, after Hack your Badge. Actual framebuffer captures show
  battery, brightness, date/time, Connect phone, Free/Default/180° and six page
  dots within the circular display. Button-label backgrounds match their fills.
- Through the production USB input dispatch, brightness changes apply
  immediately, clamp to 10–100%, and three rapid changes produce one delayed
  preference write. Minimum brightness plus Default survive restart; maximum
  brightness plus 180° survive restart. Default reports rotation 2 and 180°
  reports rotation 0. Free and 50% are restored and persist on the final boot.
- Out-and-back drags over brightness, Connect phone and orientation do not
  activate those controls. An orientation touch does not select on press.
  Pusher dispatch follows the displayed fixed orientation. These are simulated
  inputs running actual firmware, not physical finger/pusher/IMU measurements.
- Connect phone enters the temporary AP and cancellation returns to Settings
  with Wi-Fi off. Setup entered from the empty Badge still returns to Badge on
  cancellation. Successful Save return is supported by source review; it was
  not exercised by a live profile save in this test.
- Six-page navigation, animation changes, schedule scrolling, all empty network
  slots, modal setup, Hack QR decoding through the circular aperture and reboot
  restoration pass. The shipped schedule remains untimed and has no false
  current-item highlight.

An initial recheck of the polished artifact stopped on a brightness assertion.
A subsequent diagnostic showed 27 physical input events since boot; USB input
dispatch does not increment that counter. Their timing relative to the failed
assertion was not captured, so physical interference is only a possible cause.
A bounded brightness probe and complete Settings rerun then passed. No firmware
change was made to obtain that pass, and no specific defect was established.

Final private device results/captures are under
`.build/settings-verification/final/` and
`.build/settings-verification/final-six-pages/`. The device is left on init(),
Free orientation, 50% brightness, valid RTC, unchanged empty profile, and both
radios off. No serial monitor or temporary AP is left running.

## Phone clock acceptance limit

One bounded direct-HTTP test was attempted from the Mac. The hotspot association
command did not complete successfully, and the badge transport recorded zero
accepted HTTP requests. The test never reached the setup page or `/clock`.
This is not a clock-endpoint failure result and does not establish a working
phone/browser flow. The temporary AP was closed, its test network was removed,
and normal Mac networking was verified using a non-badge address/default route
and successful public HTTPS. No workstation security/privacy setting changed.

Phone clock synchronization, cancellation after a real phone sync, and later
reopening with a fresh real-phone clock remain device acceptance work. The
earlier profile/photo save, edit/prefill and personalized reboot limitation also
remains; see [the scaffold report](conference-verification-2026-09-16.md).
No synthetic or personal profile was written by these Settings checks.

## Host and source verification

The complete `scripts/test.sh` suite and final firmware build pass. ASan/UBSan
and production-code fixtures cover:

- Brightness bounds and hardware mapping; versioned preference validation;
  debounce, monotonic rollover, failed-save retry; exact orientation mappings;
  fresh automatic settling in all four directions and release/drag safety.
- UTC schedule start-inclusive/end-exclusive boundaries, gaps, untimed/invalid
  entries, midnight, invalid clock and forward/backward correction. Display
  offset is absent from interval comparisons; local-date rollover is separately
  covered in the clock helpers. Selection never changes scroll position.
- The authorized bounded clock endpoint, strict payload types/ranges/timezone
  validation, RTC/system-clock readback, offset persistence and failures,
  unchanged-offset write avoidance, AP continuity, and profile/staged-image
  isolation. Successful clock synchronization survives profile cancellation
  and reload in host fixtures.
- The actual rendered browser script executes in a host VM: automatic clock
  submission without profile Save, current epoch sampled again on retry and
  reopening, east/west UTC-offset signs, timezone validation, visible failures,
  unlocked controls after failure, and successful retry. Browser DOM/fetch and
  time are simulated, so this is not a physical phone or browser-network test.
- Existing navigation/profile/transport/connected-code regressions, twenty
  Python clock-provisioning tests, eight layout/USB-identity tests and seven
  flash-orchestration tests remain passing.

An independent read-only review found no actionable Settings/clock blocker.
It checked the control geometry, preference storage, release-gated rotation,
setup return behavior, clock/profile separation and UTC schedule logic.
The final visual polish was rebuilt, installed and recaptured. Physical control
feel, moved-board rotation, real-phone captive-browser behavior, battery runtime,
clock drift and full-power-loss retention remain unmeasured.

Source guides and diagnostics were updated with the implementation. No backend,
Cloudflare deployment, remote push or factory migration is part of this change.
