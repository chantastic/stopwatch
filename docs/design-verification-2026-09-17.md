# init() design verification — September 17, 2026

The supplied conference screens and isolated brand exports are implemented in
the canonical `chantastic/stopwatch` checkout, based on `f3e044b`. The tested
application identifies as `conference-factory-3`, design `init-2026`. This report
describes one development StopWatch, with host simulation distinguished below.

## Build and installation

- Pinned ESP-IDF 5.5.4, LVGL 9.5.0, Smooth 2.12.1 and Mooncake 2.3.3.
  Native widgets retain navigation, scrolling, QR creation, tap arbitration,
  image/font rendering and animation. There is no additional player framework.
- Application size: **2,386,576 bytes** (`0x246a90`) in the existing 3,145,728-byte
  slot; **759,152 bytes / 24% free**.
- Application SHA-256:
  `efac2238e4a0a8ba07d5b931ee27d6ca351285b9e2d26065f25b4dae2abe938f`.
- The partition preflight matched the attached board. Upload write hashes and
  the clock/storage readiness gate passed. No filesystem or NVS erase or
  initialization was performed. Fresh computer time was verified against RTC.

## Host checks

The complete `scripts/test.sh` suite passed during implementation, including
portable regressions, native board/clock/services/UI checks and bookmark storage.
The final UI and animation suites passed again after the 6-fps refinement.

- Exact alpha masks package the supplied small/large logos, arrows and bookmark
  without resampling. The empty portrait comes from the supplied empty design;
  personal photos remain dynamic. Rendered pages were visually compared with
  the exports. The customization, synthetic Wi-Fi and social QR payloads decode,
  including inside the circular aperture.
- Production LVGL views exercise page navigation, release-only taps, rejected
  drag/hold taps, schedule scrolling/bookmarks, configured/social-only profiles,
  native social scrolling, expanded QR lifecycle and avatar replacement.
- Optional company storage covers legacy v1 records, v2 metadata with photo/URL
  preservation, atomic-write failures, corruption, field bounds and escaping.
  Browser tests cover submission, explicit clearing and retained failed edits.
- Bookmark tests cover index/version validation, debouncing, clock wrap and
  failed-save retry without modifying clock or profile data.
- The real LVGL GIF implementation, under AddressSanitizer/UndefinedBehaviorSanitizer,
  matches independently decoded references for all 60 frames and their delays
  over repeated ten-second loops. Disposal behavior and parent deletion releasing
  animation timers are covered. Three narrowly scoped upstream fixes use the
  existing dependency-patch mechanism; see the [loop notes](../firmware/factory_badge/main/ui/intro-loop.md).

## Connected-board checks

USB diagnostics and native framebuffer captures passed on the final application:

- Existing profile-presence indicators, configured-account mask, brightness,
  orientation, After Dark unlock, storage and clock survived the update/restart.
  Personal profile and setup credential screens were not captured.
- Intro frames change. The public customization QR decodes through a circular
  mask. Schedule, invitation placeholder and Settings render at 468×466.
- A bookmark was toggled, saved, verified after restart, and restored. Brightness
  and both fixed rotations dispatched correctly and were restored.
- Injected touch-test coordinates remained explicitly labeled simulated.
  Phone setup entered temporary AP mode; leaving shut Wi-Fi off. Bluetooth
  remained off. Final state is the animated intro with original preferences.

The native-resolution 12-fps trial left only 15 main-loop ticks over two idle
seconds, with a 141-ms maximum loop gap. The final 6-fps, 60-frame asset left
**79 ticks over two seconds**, with a **133-ms maximum gap**. Lowering redraw
frequency improves time available to input handling while preserving the complete
source sequence and its encoded ten-second duration. This is a short diagnostic
sample, not a physical-panel frame-rate or complete playback-duration measurement.
The seven-shade palette and reduced frame rate fit the existing partition; the
source video is 60 fps. Native resolution retains its small cross pattern.

## Remaining limits

The actual After Dark invitation URL is pending. Its area is deliberately
unscannable; the mockup's repeated customization QR is not treated as an invite.
The real phone screen uses generated Wi-Fi credentials instead of that sample.

The isolated branding shapes are exact. This initial verification used
Inter/Roboto Mono selected from raster references. The later user clarification
specifies IBM Plex Mono Medium/SemiBold and Suisse Intl Medium, approximated by
Inter Medium; see [current font provenance](../firmware/factory_badge/main/ui/fonts/README.md).
Source Figma text styles were not inspected in that initial verification. The
[source file](https://www.figma.com/design/H0uyTnXh5IkKwiZRTL7QRo/init---conf-2026?node-id=7307-612)
was subsequently inspected in the user's logged-in Chrome Work window. Current
font provenance records the exact source roles and native-pixel approximations.
Glyph coverage remains Latin-1. The real published agenda replaces
mock session text and room names.

These checks do not establish physical fingertip alignment, physical panel
refresh, a real phone's full company/photo save flow, battery runtime or
power-cut resilience on this revision. Native rendering/input and production
storage/browser tests cover their respective paths. Existing touch/rotation
drivers and partition boundaries are unchanged. Older firmware cannot read
company-bearing v2 profiles; clear company before downgrading if required.

Private evidence is under `.build/init-design-verification/` and the
`.build/design-*-tests.log` / `.build/design-*-final.log` files. Generated firmware,
raw diagnostics, captures and the source MP4 remain outside Git.
