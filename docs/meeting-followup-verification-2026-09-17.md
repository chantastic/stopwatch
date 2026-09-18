# Badge meeting follow-up — September 17, 2026

This follow-up applies the meeting notes to native firmware based on `975b549`:
**Hack this device. / Learn how**, clearer local photo setup, a confirmed badge
reset, 60% default brightness, and 12-hour displayed clocks. ESP-IDF/LVGL/
Smooth/Mooncake, partition boundaries, clocks and profile formats remain in place.
Existing saved brightness is respected; the attached development unit is at 60%.

## Host and browser verification

The full `scripts/test.sh` suite passed. After final review found and corrected
a failed-Cancel/photo-preparation race, the affected portal clock, profile and
photo tests passed again, as did the real browser integration check.

- Native UI tests cover the renamed page, 12-hour Settings date, 60% initial
  model, reset confirmation/cancellation, rejected drag/hold/press-only input,
  duplicate confirmation, in-progress navigation, failure/retry, completion and
  reopening as a fresh confirmation. Rendered Settings/reset screens were
  visually checked inside the native display geometry.
- Clock tests cover midnight/noon, AM/PM transitions, unpadded hours, fractional
  offsets, ±14-hour offsets and date/year rollover. UTC/RTC values and daily
  agenda selection are unchanged; agenda labels already used AM/PM.
- Production profile reset runs on the existing worker and atomically writes
  an empty manual record. Synthetic filesystem tests cover all fields/photos,
  rejected setup/concurrent requests, failed open/sync/rename, explicit retry,
  durable empty readback and an untouched legacy-data sentinel.
- The actual main reset coordinator is compiled against bounded host adapters.
  It covers each individual NVS write and commit failure, duplicate requests,
  preference-only retry, a new profile invalidating that retry shortcut, and
  preserved clock/unlock data. NVS writes are modeled as immediate individual
  writes; the reset does not pretend to be one transaction across profile and
  preferences. Partial failure is shown explicitly.
- Browser script tests cover square preview before transmission, source-image
  lifetime, supported format paths, FileReader/JPEG fallbacks, bounded failures,
  selection races, image-token gating before Save, and no automatic uncertain
  save retry. Failed/timed-out Cancel preserves photo preparation; confirmed
  cancellation discards late results.
- A fresh isolated desktop Chrome **153.0.8010.50** passed real native
  Image/FileReader/canvas/JPEG conversion with a synthetic PNG against a local
  HTTP fixture. Crop pixels, JPEG dimensions/type/size, Content-Length and
  `/image` → `/save` ordering passed, including fallback conversion paths and
  malformed-image rejection without losing draft edits.

## Firmware artifact

- Application: **2,400,224 bytes** (`0x249fe0`), with **745,504 bytes / 24% free**
  in the unchanged application slot.
- SHA-256:
  `429a120c2f3729cfb88ff02a16a649c35a8dcbffd2b27e8df37a98408454f513`.
- Runtime identifies as `conference-factory-3`, design `init-2026`, with new
  non-personal `reset_active` and `reset_state` diagnostic fields.

## Connected development badge

The final artifact above uploaded with a matching partition preflight, verified
write hashes and successful fresh RTC/storage readiness. The flash did not format
or initialize the filesystem, erase NVS, or confirm a badge reset.

USB diagnostics and native framebuffer checks passed:

- Saved profile-presence indicators, configured accounts, bookmarks, brightness,
  orientation and event unlock survived flashing/restart. Settings shows 60%
  brightness and the actual local `h:mm PM` clock. Public artwork and the renamed
  hacking page rendered; its QR decoded inside the circular aperture.
- Opening reset shows its confirmation. A pusher cancels it, and a drag through
  the Settings row does not open it. All personal-data indicators and bookmark
  state were preserved. The destructive confirmation was never pressed.
- A reversible bookmark change persisted across restart and was restored.
  Brightness changes and both fixed rotations worked and were restored.
- Injected Touch test input retained its simulated label. Setup entered AP mode
  and closed back to offline operation. No setup credentials or personal badge
  face were captured.
- The intro still animates; the short idle sample measured 76 main-loop ticks
  over two seconds and a 134-ms maximum gap. Final state is the intro page,
  original preferences, Wi-Fi off and Bluetooth off.

These are software/USB/framebuffer observations, not physical fingertip or
physical-panel acceptance.

## Limits

Desktop browser and simulated portal tests do not establish the reported phone's
captive sign-in picker behavior, actual HEIC decoding, cloud-only photo access
or phone-to-badge transfer. The portal provides explicit normal-browser guidance:
stay on the badge Wi-Fi and open `http://192.168.4.1` in Safari or Chrome.
No phone model/OS was available during this run. Real-phone verification remains.

Confirmed erasure is tested only with synthetic records; the personalized
development badge must not be cleared just to test this feature. Physical touch
alignment, screen refresh, flash power cuts and battery runtime are separate
hardware checks. Public firmware releases and website pins are unchanged.

Private evidence stays in `.build/meeting-verification/`, `.build/meeting-*.log`
and the ignored factory-services/reset/UI test outputs. No personal profile,
photo, original video or raw device log is included in Git.
