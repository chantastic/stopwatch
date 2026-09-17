# After Dark reveal verification — September 17, 2026

> The results below describe the initial daily-cutoff build `bb366d3`. The user
> subsequently clarified that the reveal must use the fixed October 7, 2026,
> 13:30 local cutoff. See the date-cutoff correction section below for current
> results. The initial build's stored unlock is preserved on the development
> badge; its already-unlocked state cannot verify fresh-before-event hiding.

This development change adds the hidden invitation to the native factory stack.
Its readiness protocol remains `conference-factory-3`; it is not a new published
firmware release. The installed application is 1,678,752 bytes, SHA-256
`83e5dcd071caf66b0fe8f0bb36fb8b75c8a6e4488693bf301352e8c825d1e90a`.

## Automated host checks

- Full `scripts/test.sh` suite passed with the existing sanitizers.
- Morse tests cover either pusher, all 8,192 seven-pulse/letter-grouping
  combinations, final-letter pause, noise, malformed words, long holds,
  switching pushers, chords, modal cancellation, timeout and millisecond wrap.
- Reveal-policy tests cover the exact local 13:30 boundary, positive/negative
  offsets, repeating dates, invalid and corrected clocks, versioned persistence,
  failed-save retry and monotonic wrap.
- Real LVGL/Smooth/Mooncake fixtures cover locked navigation in both directions,
  horizontal swipe, five/six dots, preserved schedule objects/scroll on reveal,
  invitation entry, modal isolation, existing views and idle rendering.
- The navigation fixture exposed an existing gesture-routing omission: LVGL
  bubbled gestures beyond the scene handler. The scene now owns them, and clears
  pending gestures on new/cancelled contacts. Final targeted UI tests passed,
  including release over chrome and framework cancellation with no later page jump.
- Final native ESP-IDF build passed after that gesture fix.

These simulated inputs do not establish physical Morse ergonomics or sensor
alignment. No physical Morse code was entered during this verification.

## Connected development badge

The normal flash wrapper verified the existing partition sector and upload
hashes, provisioned the computer's fresh time, checked the advancing hardware
RTC, and reported `UNIT_READY`. No storage initialization or erase was requested.

The badge started after local 13:30 and reported six visible pages with the
reveal committed. USB verification then moved its clock back to 09:00, restarted
it, and confirmed the invitation remained unlocked and reachable. Brightness,
orientation, selected network and profile-presence indicators remained unchanged;
storage remained available and Wi-Fi/Bluetooth remained off. Forward navigation
visited all six stable page IDs and wrapped back to init().

The correct computer time and original display offset were restored. The badge
was left on After Dark. Its framebuffer capture was inspected for title, footer
and dots; physical panel refresh and finger/button feel were not independently
observed. The exact first threshold crossing and locked navigation were tested
on the host; this device run verified reveal on an already-afternoon clock.

Private evidence is under `.build/after-dark-verification/`, alongside
`.build/after-dark-{build,tests,flash}.log` in the WorkOS checkout. Binaries,
captures and diagnostic output are not committed. The public installer and Alto
deployment remain unchanged.

## Date-cutoff correction

The current policy uses October 7, 2026, 13:30 in the badge's configured local
time as a fixed cutoff. Fresh instances unlock at every later time, including
the following midnight/morning; earlier evenings remain hidden. The existing
versioned NVS key/encoding is preserved so earned unlocks are retained.

The revised policy passed ASan/UBSan checks for every minute across nine dates,
every following-day hour across six offsets, the exact second before/at the
cutoff, invalid clocks/offsets and integer extremes. Existing persistence and
monotonic retry checks also passed. These are host simulations of fresh badges.

The corrected native build passed and was installed through the guarded flash
wrapper, including partition/hash checks, fresh clock/RTC provisioning and
`UNIT_READY`. Application size is 1,678,864 bytes, SHA-256
`68226dc9105aa61acf8801e6e35fa0d67a91e781756e943c7e77ac6076a2eba8`.
Fresh USB status confirmed the development badge retained its previously stored
unlock, clock and storage were ready, and both radios were off. It was left on
After Dark. No saved state was reset to manufacture a fresh-device test.

Private evidence: `.build/after-dark-date-{build,flash}.log` and
`.build/after-dark-verification/date-correction.json`. Public release and website
pins remain unchanged.
