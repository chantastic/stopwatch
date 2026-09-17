# Project guidance

## Start here each session

- Read [hardware.md](docs/hardware.md) for the exact board, display geometry,
  pin mappings, orientation calibration, memory layout, and USB quirks.
- Read [decisions.md](docs/decisions.md) for the user's choices and the failures
  already resolved. These notes describe the current design, not new feature work.
- Use [development.md](docs/development.md) for builds, device checks, diagnostic
  protocol, and recovery. [architecture.md](docs/architecture.md) explains the
  firmware/backend contract; [privacy.md](docs/privacy.md) explains storage.
- If `.local/project-context.md` exists, read it for this workstation's backend,
  original backup, and private verification locations. Keep it out of Git.
- Check the current branch/worktree and connected port before making changes.
  The recorded hardware baseline is dated; it is not a live device status check.

## Established constraints

- Canonical team source is private `workos/stopwatch`: firmware at the repository
  root, browser installer in `web-flasher/`, and Alto website under `site/`.
  Read `CONTRIBUTING.md` and `docs/site-promotion.md` for review and release rules.
  Alto's managed `stopwatch` Git repository is a publication target; edit `site/`
  here and promote a reviewed main commit. A merge here does not deploy.
- Current approved public firmware files remain in the original GitHub release
  repository, pinned by exact hashes in `site/src/release-config.json`. Moving
  team source does not change those files or make private GitHub assets available
  anonymously. New firmware needs an explicitly qualified release before updating
  the website pins; never promote an untested binary because its host checks pass.
- Active firmware is the native ESP-IDF project in `firmware/factory_badge/`.
  Read [factory-stack.md](docs/factory-stack.md) for the LVGL/Smooth/Mooncake
  architecture, per-page views, hardware boundary and pinned dependencies.
  `firmware/devices_badge/` is the retained Arduino implementation and shared
  portable helpers; do not implement new views there.
- Current product is the **offline conference badge**. Read
  [conference-badge.md](docs/conference-badge.md) and
  [conference-clock.md](docs/conference-clock.md). The six-page UI supersedes
  the connected badge/voice controls below; that previous application is retained
  in `legacy_connected_app.h` and Git history and is not compiled into this build.
- Normal operation has Wi-Fi and Bluetooth off. Temporary setup is AP-only,
  with manual name/social fields and image upload. Never load authenticated
  identity caches into the manual conference model or require a cloud login.
- Physical screen-left/right pushers are previous/next at rotations 0 and 2;
  loop-up rotation 2 means blue previous / yellow next. Both open setup; either
  exits setup. Preserve completed taps, drag arbitration, and orientation filtering.
- Settings is the final page, after Hack your Badge. Brightness has a visible
  minimum and delayed persistent saves. Orientation is exactly Free (automatic),
  Default (rotation 0), or 180° (rotation 2); apply changes after touch release.
  Phone setup synchronizes a fresh browser clock independently of profile Save
  and Cancel. Settings-launched setup returns to Settings. The published agenda
  in `factory_badge/main/schedule.h` repeats daily using the badge's local clock:
  current rows say On now, passed rows are dimmed, and invalid time marks neither.
  Keep input/setup/save deadlines on monotonic time, independent of clock changes.
- After Dark is hidden from navigation and dots until either pusher enters Morse
  `init` (`.. -. .. -`, one pusher per attempt) or valid local time reaches 13:30
  on any day. Persist the reveal in its separate versioned NVS byte. Timed reveal
  must not move the current page/scroll; code success opens the invite. Setup,
  Touch test and chords cancel recognition. Keep stable page IDs and ordinary
  pusher paging. See `docs/conference-badge.md` for timing and persistence rules.
- Settings → Touch test is an observation-only modal: five white crosshairs,
  live purple sensor marker retained on release, and current rotation frozen
  without changing the saved mode. Either pusher or the chord returns to Settings.
  Never apply calibration, save touch data, or start networking from this test.
  Native CST820 coordinates feed LVGL directly; LVGL owns touch rotation.
  The factory board adapter translates M5 rotation numbering once and keeps
  its 468×466 native display geometry. Do not reintroduce the retired Arduino
  scale/offset trial. See hardware.md for its historical fit limitations.
  Label simulated inputs; they cannot verify physical sensor alignment.
- Build once for a batch; each flash provisions and verifies a fresh hardware RTC
  time and storage/radio readiness. Preserve the partition layout and user state.
  Every flash must first compare the current partition sector with the compiled
  map. Factory/different layouts require a separately authorized migration.
  Explicit filesystem initialization applies only to the verified existing map
  and erases ONLY `ffat`; never run it on the provisioned development board or add
  automatic format-on-error.
- Active gateway source is `/Users/chan/Developer/chan-services/apps/devices`.
  The private `chantastic/chan-services` monorepo owns service deployments; read
  its `docs/deployment.md` before gateway work. `gateway/` here and the standalone
  `../devices.chan.dev` repository are retained historical snapshots. Do not
  implement or deploy gateway changes from those copies. Firmware stays here.
- The retained connected prototype intentionally connects to chan.dev Production
  Devices application. Any signed-in Production user uses their own account and
  personal connections. Shared Auth, Pipes integration, Social and the narrow
  Devices gateway share the services monorepo but retain separate Workers and
  private bindings. Do not build a separate authentication stack or move device
  workflows into Auth or Social.
- Hardware is an M5Stack **StopWatch**, ESP32-S3 with 16 MiB flash and 8 MiB OPI
  PSRAM. Use the pinned build configuration; do not trust USB board-name guesses.
- Keep public client IDs and HTTPS endpoint configuration separate from secrets.
  Wi-Fi credentials, session tokens, and cached profiles are provisioned or saved
  on the device, never baked into source.
- Keep full-flash backups, NVS dumps, live diagnostic logs, screenshots, and
  generated binaries outside Git. Build and test output goes in `.build/`.
- `scripts/build.sh` compiles; `scripts/test.sh` runs the host checks on macOS.
  Use the dependency versions in README.md.
  LVGL and hardware state belong to the main task; HTTP/DNS run on workers.
  Phone clock writes are marshalled back to main. Update view models through
  callbacks; pages must not access hardware, NVS, Wi-Fi, or profile storage.
- `scripts/flash.sh PORT` rebuilds and uploads application components. Firmware
  changes should preserve the partition layout and saved user state.
- Avatar and profile behavior must remain dynamic. Avoid static personal assets.
- Historical connected application: one `init()` ASCII layout per account;
  blue changes accounts and yellow is reserved. Both open Settings. Those rules
  are not the conference controls. Touch uses completed taps on release.
- The historical X replies app opens from Settings and owns its blue-hold recording
  gesture. Read [voice-replies.md](docs/voice-replies.md) before changing this flow.
  Sending requires a fresh tap on Send after reviewing every transcript page.
  Preserve unresolved send receipts across Back, sign-in changes, and restart;
  never automatically retry a post or replace an uncertain receipt key.
- Microphone capture and upload stay on workers. No recording at startup; no
  permanent provider keys on the board; no audio or transcripts in Git/logs.
  Tests may transcribe a bounded recording, but a real public reply requires the
  user's exact approved target/content or their deliberate Send on the device.
- Keep HTTPS waits on the worker and display/state ownership on the main task.
  Restore cached profiles before networking; a cache must never authenticate a user.

## Keep this memory useful

- When hardware behavior, controls, integration contracts, or verified limits
  change, update the relevant guide in the same change. Keep this file concise.
- Label measured results with their source version/date and distinguish host
  simulations from device observations. Preserve limitations and failed approaches
  that explain a current implementation choice.
- Do not copy transcripts, historical artifacts, or raw diagnostics into these
  guides. Record the durable finding and link to current code or a private locator.
