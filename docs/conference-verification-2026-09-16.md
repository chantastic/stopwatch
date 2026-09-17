# Conference scaffold verification — September 16, 2026

Primary worktree: `/Users/chan/Developer/m5stack-conference-badge`, branch
`codex/offline-conference-badge`, based on `b459f26`. Runtime build label:
`conference-scaffold-1`. The final local artifact digest and sanitized readiness
record are in ignored `.build/conference-verification/release.json`. This is one
development device, not a 400-unit production qualification.

Final application: 1,282,607 compiled code bytes, 54,668 global bytes;
1,282,752-byte upload artifact. Application SHA-256:
`d684b1da37912ba715fc727a09123cb03deb7ff9dde51f088c8898eb900d2f0a`.

## Actual device observations

- StopWatch board ID 30; 16 MiB flash and 8 MiB PSRAM reported correctly.
- The guarded uploader read the live partition sector, matched it to the compiled
  table, and resolved the same USB identity across resets before proceeding.
  No full erase, partition migration, NVS/filesystem image, or explicit storage
  initialization was performed by this worktree's flashing workflow. Existing
  LittleFS mounted successfully; the conference profile remained blank.
- USB provisioning wrote fresh computer time, read back the RX8130 hardware RTC,
  waited and confirmed that it advanced. The final readiness response required
  storage/clock/hardware checks and both radios off outside setup.
- Actual 468×468 framebuffer captures show all five pages. Animation frames
  differ over time. The Hack QR decodes to the required destination both in the
  full frame and after applying the round aperture. The invite and empty Badge
  placeholders contain no decodable QR. The reviewed contact sheet is
  `.build/conference-verification/final/five-pages.png`.
- USB commands exercise the same navigation dispatch: five-page wrapping in
  both directions, rotation-aware pusher mapping, schedule scroll limits, all
  three empty network slots, modal setup, tap/drag arbitration, setup cancellation
  and radio shutdown. These are **simulated inputs on actual firmware**, not a
  physical pusher or finger test.
- Restart returns to init(), preserves the selected network, reloads a valid RTC,
  and leaves both radios off. Empty profile state stays empty.

## Local setup: what was and was not established

The Mac repeatedly associated with the temporary AP: the device reported one
client, and the Mac held an address on the badge subnet. Actual `GET /` requests
returned the setup HTML with HTTP 200, including native captive-probe requests.
The device's bounded transport diagnostics recorded completed responses.

The workstation could not complete the full browser flow reliably. Chrome
reported offline/network-change errors; separate HTTP clients intermittently
failed to connect despite association. The slow-upload client reported closure
after about half a second, while device diagnostics showed the POST still open
in body reception with 0/4096 bytes. It closed when the test explicitly cancelled
setup. This is not evidence that the ten-second device deadline failed, nor is
it a successful live deadline test. Its cause remains unresolved.
An independent read-only review against the pinned ESP32 Arduino 3.3.10 SDK
found no specific socket-ownership, body-buffering or deadline defect. That
source review does not establish the cause of the observed connection failure.

No real-device profile save, image upload, edit/prefill, or personalized-profile
reboot test succeeded. No synthetic profile was committed. The browser HTTP
relay option also did not complete; it must not be described as verification.
Every test cleanup closed the temporary AP and requested restoration of normal
Mac Wi-Fi. No Mac security/privacy settings were changed.

The remaining acceptance test is to use a phone or a host with a stable local
connection, save a synthetic name/photo/one network, confirm the two empty slots,
restart, reopen setup and confirm prefill/photo retention, add the other two
networks, cancel a later edit, and decode each displayed QR. Finally remove only
that test profile. Run the slow-upload/cancel check from a stable client too.

## Host verification

`scripts/test.sh` passes the existing retained-application regressions and the
new conference suites. ASan/UBSan checks cover:

- Navigation, scrolling, network-slot isolation, modal state and completed taps.
- Actual JPEG decoding and bounded dimensions/size, UTF-8/name/URL validation,
  isolated profile records, atomic-save failure injection, corruption rejection,
  edit/reload, image keep/remove and clear.
- The production HTTP transport over real host sockets: authorized byte-per-
  500-ms uploads remain responsive and close at the absolute deadline; immediate
  stop cancels sockets and setup; header/idle/session deadlines, a 2 KiB per-tick
  transfer bound, and stalled response readers are covered.
- Production portal handling: nonce/header/body rejection, staged-image save,
  escaped prefill, cancel, failed storage commit, acknowledgment grace and
  rollover, failed server-start cleanup, and safe transport diagnostics.
- RTC calendar validation, oscillator-loss flag, read/write/NVS failures and
  nonce-bound command handling. Twenty Python provisioning tests, eight layout/
  USB-identity tests and seven flashing-orchestration tests pass.

Host fixtures simulate Wi-Fi, device storage and RTC; they do not establish
physical RF behavior, real flash persistence of a personalized profile, or
phone captive-browser behavior. Portal JavaScript is syntax checked; that is not
an end-to-end browser assertion.

## Remaining physical and release limits

Physical touch/pusher timing, movement-driven orientation, phone QR scanning,
iOS/Android captive-browser behavior, battery runtime, clock drift and retention
after full power loss remain unmeasured. The schedule, invite destination and
final animation artwork are intentionally placeholders. The separate factory
partition migration procedure is a documented design, unimplemented and
untested on factory units; see [the batch runbook](conference-clock.md).

Private evidence stays in `.build/conference-verification/`, with build, host
test and upload logs in `.build/`. Do not commit device dumps, diagnostic captures,
profile images, setup credentials or logs.
