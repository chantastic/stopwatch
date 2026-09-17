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

## Published release

The public GitHub prerelease is
<https://github.com/chantastic/m5stack-stopwatch-authkit/releases/tag/conference-factory-3>,
tagged from source commit `598113b`. It contains exactly the manifest, bootloader,
partition table and application; no device readback, NVS or filesystem image.
GitHub reported the expected byte sizes and SHA-256 digests for all four assets.

The first Alto deployment served the installer but its runtime GitHub proxy
returned 502 for all four files. Node and local workerd downloads passed. The
installed Alto SDK documents that managed outbound requests pass through an
organization/platform host allowlist. Release acquisition was therefore moved
to build-time hash verification and ignored generated site content, keeping the
existing outbound policy intact.

StopWatch Alto release 8 (`01M2RHPV071C9QDM8AEE9AX7C3`), source
`4ab7b1d0908bdf2359f083d1520d9f6e9125a9f5`, then passed anonymous production checks:
the guide, installer and static assets returned 200, and all four release files
matched their approved sizes and SHA-256 digests. Versioned release files have
immutable caching. The live installer rendered correctly with no browser console
warnings or errors; its bundle is unchanged from the physical browser test.

Drops Alto release 1 (`01M2RHSX06NZJ86Y23ZSMN20GP`), source
`882f82826f378cac3617634c3b86500d9081082d`, serves the requested entry
<https://drops.workos.cloud/stopwatch>. Anonymous requests redirect to the live
installer with 302, then 200. A browser navigation through that exact entry
rendered the install, release-download and issue-reporting controls with no
console warnings or errors. The trailing-slash alias also works; query parameters
are discarded and unknown subpaths return 404.

The existing singular guide, <https://drop.workos.cloud/stopwatch>, was updated
to live version 2 with the standalone installer link. Its organization audience
and editors-only editing policy are unchanged. The full generated guide bundle
hash matched Alto's staged hash; the platform's truncated source readback was
not used as the publishing source.

## Limits

This verifies one physical StopWatch in Chrome on macOS. It does not qualify
Windows, Linux, Edge, stock partition conversion, secure/encrypted devices,
battery runtime, or power interruption mid-flash. Factory layouts remain
blocked before writes; no chip erase or filesystem initialization was tested.
The existing profile was not replaced with synthetic personal data for testing.

The physical integration test used local serving; production publication was
checked separately by UI inspection and exact asset hashes, without a redundant
second flash of the same application.
