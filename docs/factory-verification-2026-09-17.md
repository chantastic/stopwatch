# Factory-stack verification — September 17, 2026

The initial native ESP-IDF application, `conference-factory-1`, was built and
installed on the attached StopWatch. Source is the migration worktree on
`codex/offline-conference-badge`, based on `0082591`; the private source manifest
records the exact files tested. This report supersedes Arduino acceptance results
for the migrated UI and drivers.

**Subsequent physical-display failure:** the first build's successful framebuffer
checks did not establish panel refresh at 180°. The user later reported a frozen
startup screen. See the follow-up below before using the initial results.

## Build and installation

- ESP-IDF 5.5.4, LVGL 9.5.0, Smooth UI Toolkit 2.12.1, Mooncake 2.3.3,
  M5GFX 0.2.19; exact dependency revisions are pinned in the project.
- Application: **1,676,544 bytes** in the existing 3,145,728-byte slot.
- Application SHA-256:
  `d6987a56764b90fa66565bdd84890ca01ade8942b3b5b04833aecb5a5d0b087c`.
- Existing partition table matched before upload. Component write hashes passed.
  Neither NVS nor the filesystem was erased or initialized.
- Fresh computer time was provisioned and verified against the advancing RTC.
  The readiness gate reported native build, available storage and offline radios.

## Checks that passed

**Host checks:** the complete `scripts/test.sh` suite, including retained portable
regressions and native board, clock, services and UI tests. Board tests were
rerun after the final touch-provenance correction.

- Production rotation adapter against real LVGL: 872,352 coordinate checks
  across four orientations; checked RTC date/BCD/STOP/VLF/error behavior.
- Simulated touches retain their simulated label through idle sensor polls;
  physical provenance begins only with an actual physical coordinate.
- Native/Arduino profile-record compatibility in both directions, including
  avatar pixels; corrupt records and failed writes preserve the prior record.
  Explicit storage initialization guards reject incompatible or busy storage.
- Real portal browser script sends fresh clock values and reports failures;
  production clock queue keeps hardware access on main and returns actual RTC
  readback. RTC/system/NVS failures, midnight rollover and UTC offsets are covered.
- Production LVGL views with Smooth/Mooncake: six pages, dynamic avatar/social
  cards, QR expansion, schedule/social scrolling, modal lifecycle, partial
  rendering and shared tap arbitration. Static pages produce no idle redraws.

**Connected board:** `scripts/verify-factory.py` passed after upload.

- All six pages captured at native 468×466; fixed clock/pagers/footer remain
  present. The Hack your Badge QR decodes inside the circular screen aperture.
- The init animation changes frames. Schedule and invite remain placeholders.
- Dragging off and back onto Settings controls does not activate them.
- Brightness changes and restores; Default and 180° apply correctly. Injected
  center touches arrive at `(234,234)` in each and remain labeled simulated.
- Temporary AP starts; leaving setup shuts it down and returns to Settings.
  Capturing the setup screen is rejected because it contains credentials.
- Restart restores brightness, orientation, network selection, storage readiness
  and RTC time with radios off. The board had no personal profile before or after.
- Final state: 50% brightness, Default orientation, Touch test open, radios off.

Review also corrected a held-finger-at-boot case: if rotation is deferred, the
saved fixed mode is retried on release. This edge case was source-reviewed;
it has not been physically reproduced on the board.

## Follow-up: physical display frozen at 180°

Live observation found a responsive main loop and eight button actions, with
page state changing from init to Settings to Hack your Badge. Capturing RAM
showed the correct Hack your Badge screen. A two-minute log contained no panic
or restart. The saved orientation was 180°. These observations distinguish the
reported display freeze from an observed processor crash.

The source cause is in pinned M5GFX 0.2.19
`Panel_FrameBufferBase::writePixels`: rotation reverses rectangle endpoints,
then records them as dirty bounds without normalizing them (quarter turns also
omit an axis swap). At 180° a full-width write produces left 467 / right 0.
`Panel_AMOLED_Framebuffer::display` rejects that empty rectangle even though
the framebuffer pixels have already changed.

`conference-factory-2` replaces the board flush with `pushImage` in complete-row
chunks of at most 8,192 pixels. Its `writeImage` path rotates both pixels and
dirty rectangles correctly. No touch calibration, saved data or partition changes
are involved. The new application is **1,676,000 bytes**, SHA-256
`481fea5a2398928aa31ba27f6b9570dcd5d2ec13f2208f4b0c1bd4d37e59a8c6`.

Private failure observations and follow-up build/upload logs are under
`.build/crash-watch/`.

Follow-up verification passed:

- The regression compiles the actual pinned M5GFX framebuffer driver. It
  reproduces the old failure, then checks 20 full/partial-region cases across
  four rotations using the production chunk helper: exact source-pixel readback,
  correct changed-region coverage and the 8,192-pixel limit. All four board
  tests pass; clock provisioning and flash workflow tests also pass.
- `conference-factory-2` uploaded with partition preservation and verified
  component hashes. Clock/storage/offline readiness passed.
- The connected-board acceptance script passed again for all pages, QR,
  animation, controls, both fixed rotations, setup lifecycle and restart.
- During subsequent live user interaction, page navigation, orientation changes
  and setup entry/exit continued without a panic or restart.
- The user explicitly confirmed that the **physical screen now updates normally,
  including at 180°**. This is physical confirmation, separate from RAM captures.

The user's later manual page/orientation selections were left intact, and the
diagnostic connection was closed. This fixes display refresh; it is not yet a
five-target physical touch-alignment acceptance test.

## Limits and next physical check

Injected coordinates and rendered frames do **not** establish physical fingertip
alignment. The previous Arduino scale/offset fit is removed. Tap the five
crosshairs in Default, leave the test, select 180°, reopen it, and repeat before
judging the new factory input path. Touch test freezes the current orientation.

The native AP lifecycle was tested, but a phone's captive-browser clock sync,
photo upload, save/edit/reopen and persistence were not exercised end to end on
this build. Their production storage/browser/clock code has host coverage.
Battery runtime, hardware power-cut behavior and batch throughput remain
unmeasured. These checks concern one development unit, not all conference units.

Private evidence: `.build/factory-logs/`, `.build/factory-verification/device/`,
and `.build/factory-verification/source-manifest.json`. Generated binaries,
frame captures and diagnostic logs stay outside Git.
