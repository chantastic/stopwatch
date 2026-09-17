# M5Stack StopWatch conference badge

An offline, locally configurable conference badge for the **M5Stack StopWatch**.
The active firmware uses the factory **ESP-IDF + LVGL + Smooth UI Toolkit +
Mooncake** stack, with the factory CO5300 display and CST820 touch integration.
No internet, attendee account, cloud backend, or sign-in is needed.

Six pages: **init() animation → Schedule → Developers After Dark → Badge →
Hack your Badge → Settings**. Schedule uses the [published init() agenda](https://workos.com/init),
repeats daily in local time, highlights **On now**, and dims passed blocks.
Invite and animation artwork remain temporary.
The Hack QR opens [drop.workos.cloud/stopwatch](https://drop.workos.cloud/stopwatch).

Browser release packaging and publication are documented in
[web releases](docs/web-releases.md). The alpha installer preserves compatible
conference badges; factory-layout conversion is not yet qualified.

Use the physical left/right pushers or on-screen arrows to page. Swipe vertically
to scroll Schedule or switch between GitHub, X/Twitter and LinkedIn within Badge.
Both pushers open local setup; either pusher returns. Empty accounts say **Tap to
configure**. Scan the setup Wi-Fi QR and edit your name, social handles, and photo
on your phone. Previous values prefill the form. Normal operation has Wi-Fi and
Bluetooth off.

In lanyard-up orientation, blue is previous and yellow is next. Completed taps,
drag rejection and stable auto-rotation remain in place. Tap a configured badge
to expand its QR; press both pushers to edit it.

Settings shows battery percentage, persistent brightness, date/time and a
**Connect phone** action. Orientation choices are **Free** (automatic),
**Default** (stock orientation), and **180°** (lanyard-up). Opening phone setup
automatically synchronizes its fresh clock, independently of saving profile
edits. A successful clock sync is retained even if those edits are cancelled.
The actual phone flow still needs the acceptance check described in the
[Settings verification report](docs/conference-verification-2026-09-17.md).

**Settings → Touch test** shows five white targets and a live purple touch
marker, retained after release. Either pusher returns to Settings. The test
temporarily holds the current rotation; it changes no saved mode or calibration,
saves no touch data, and starts no network. Choose Default or 180° and reopen it
to compare alignment. Physical alignment still requires observation on the
device; simulated diagnostic input cannot establish it.

## Build and flash

Run the one-time setup below to install **ESP-IDF 5.5.4** and fetch the pinned
factory dependencies. Builds use **LVGL 9.5.0**, **Smooth UI Toolkit 2.12.1**,
**Mooncake 2.3.3**, and **M5GFX 0.2.19**. The reviewed USB discovery/preflight/upload
wrapper still needs Arduino CLI and ESP32 core **3.3.10**; the application itself
contains no Arduino or M5Unified runtime. Host tests also require CMake, Node.js,
Clang, and the legacy ArduinoJson 7.4.3 headers for retained regression fixtures.
See [the factory stack guide](docs/factory-stack.md) for source organization.

```sh
./scripts/setup-factory.sh  # once per workstation
./scripts/test.sh
./scripts/build.sh
./scripts/flash.sh --no-build /dev/cu.usbmodemYOUR_PORT
```

The same compiled artifact can serve a batch. Each upload sets fresh UTC time
from the flashing computer, reads back the RX8130 RTC, verifies that it ticks,
and checks storage/radio/hardware readiness. The default display offset is the
computer's current local offset. See the [clock and batch runbook](docs/conference-clock.md)
for first-install storage preparation and failure handling. Ordinary flashes
preserve user storage. Before uploading, the script reads and verifies the current
partition map and follows the same USB identity across resets. A factory or
different map is rejected; factory conversion requires a separate reviewed
migration, not the filesystem-initialization option. No full-device dump,
developer profile, or photo is shipped.

## Development and evidence

- [Conference behavior and verification](docs/conference-badge.md)
- [Clock and batch flashing](docs/conference-clock.md)
- [Stock UI framework/source/license findings](docs/stock-ui-reference.md)
- [Hardware geometry and calibrated orientation](docs/hardware.md)
- [Project instructions](AGENTS.md)

Active source is `firmware/factory_badge/main/`. Each native LVGL page has its own
file under `ui/`, with shared widgets, styling and navigation. Board, profile/
portal, and clock services have separate modules. Host tests include failure injection;
physical device observations are separately labeled. Build outputs, uploaded test
photos, diagnostic captures and private logs stay in ignored `.build/`.

The former Arduino conference application remains under `firmware/devices_badge/`
for reference and regression tests. `build-arduino-legacy.sh` builds that historical
application; normal build/flash commands select the native factory stack.
The prior connected AuthKit/voice badge remains in `legacy_connected_app.h`,
[historical documentation](docs/connected-badge-history.md), and Git history.
Its cached identities are not used by the manual conference badge. Shared services
remain in the private `chan-services` monorepo; this firmware change deploys none.

Original project code has no selected license. Dependencies and brand assets have
separate terms; see [third-party notices](THIRD_PARTY_NOTICES.md).
