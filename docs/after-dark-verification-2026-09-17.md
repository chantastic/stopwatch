# After Dark reveal verification — September 17, 2026

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
