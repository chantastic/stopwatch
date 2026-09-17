# Browser release verification — September 17, 2026

Target: `conference-factory-3`, application SHA-256
`78e3c38b36cd4a1a75fd49c707f4796b522d03bfbc28fa4ca83c7c8b966cce73`.
This is the previously verified daily-agenda build; no new firmware changes were
introduced for this release. Browser installer source lives in `web-flasher/`.

## Host checks

- Existing firmware host checks passed through `scripts/test.sh`.
- 18 release-packager tests passed, plus independent verification of the actual
  three-component release and padded partition-sector hash.
- 33 browser tests passed: strict manifest/hardware/security/OTA guards,
  SHA/MD5 checks, serial stream parsing and bounds, nonces, advancing RTC and
  readiness, diagnostic redaction, unplug/write-failure cleanup.
- Installer bundled successfully with pinned esptool-js 0.6.1 and js-md5 0.8.3.

## Physical browser test

Used desktop Google Chrome on macOS, the development StopWatch at its live
`cu.usbmodem2101` port, and the exact packaged artifacts. The page was served
locally for this integration test.

1. A standalone browser clock/check action completed and displayed verified
   firmware, storage, offline mode, and an advancing RTC.
2. The first preservation preflight stopped before writing because the device
   retained the Arduino uploader's OTA bootstrap record. A read-only inspection
   established that all 8,192 bytes matched the official ESP32 core 3.3.10
   `boot_app0.bin`; pinned IDF selects app0 for that exact record.
3. The browser guard was extended to recognize that one additional exact
   fixture. Corrupt, hybrid, app1 and other OTA states remain blocked. No OTA
   metadata is written by the installer.
4. The full browser installation then passed: security and flash checks,
   partition-sector comparison, approved selector, three sparse component
   writes with MD5 verification, reset, fresh computer time, advancing RTC,
   matching build/memory, mounted storage, and offline radios.
5. Chrome displayed “Ready. Firmware, storage, offline mode, and the advancing
   clock are verified.” Controls became available again after cleanup.

The legacy bootstrap fixture hash is
`f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0`.
It contains boot metadata only. Private readback and test logs remain under
`.build/web-release/`, outside Git and published release assets.

## Limits

This verifies one physical StopWatch in Chrome on macOS. It does not qualify
Windows, Linux, Edge, stock partition conversion, secure/encrypted devices,
battery runtime, or power interruption mid-flash. Factory layouts remain
blocked before writes; no chip erase or filesystem initialization was tested.
The existing profile was not replaced with synthetic personal data for testing.

Alto publication and anonymous asset delivery require separate checks after
deployment; local browser success alone is not evidence of a live website.
