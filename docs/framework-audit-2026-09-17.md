# Factory framework audit

September 17, 2026 — reviewed the working tree on
`codex/offline-conference-badge`, including `conference-factory-3` and its daily
schedule. Findings below are open recommendations, not completed fixes.

## Conclusion

The factory stack is now doing the core UI work. LVGL owns widgets, text layout,
scrolling, snapping, QR codes, animation, hit testing, input rotation and dirty
rendering. The board adapter also sets M5GFX's matching drawing rotation.
Mooncake owns the application and Smooth owns its root container. A second UI
migration is unnecessary.

The useful next work is to finish the integration: correct gesture and callback
ownership, improve captive-portal compatibility, consolidate native build tools,
and tighten the boundary between hardware and application policy. Some custom
code remains necessary because the factory demo lacks the badge's behavior or
contains a defect already demonstrated on this board.

## Scope and evidence

- Active sources: `firmware/factory_badge/`, the portable helpers it imports from
  `firmware/devices_badge/`, and build/provisioning scripts.
- Compared against the installed sources: ESP-IDF 5.5.4, LVGL 9.5.0, Smooth
  2.12.1, Mooncake 2.3.3, M5GFX 0.2.19 and M5StopWatch-UserDemo revision
  `6b4aa125288b6fe9dca661f10159f6e1e5ee785c`. Exact framework commits remain in
  `firmware/factory_badge/frameworks.json`; managed components use
  `dependencies.lock`.
- Three parallel reviews covered UI, hardware/input/clock, and services/storage.
  A separate pass covered dependency and flashing tools.
- Host probes reproduced gesture routing, a teardown use-after-free, and a
  dependency verification gap. Logs and probes remain outside Git under
  `.build/ui-framework-audit/` and `.build/framework-audit/`.
- This audit did not access or flash the board. Source-derived risks below are
  distinguished from host reproductions. Phone compatibility and battery savings
  were not measured during this audit.

## Framework integration findings

### 1. P2 — Horizontal page gestures bypass their handler

`main/badge_ui.cpp:31` attaches `LV_EVENT_GESTURE` to the Smooth frame. That frame
retains LVGL's default `LV_OBJ_FLAG_GESTURE_BUBBLE`. In LVGL 9.5,
`src/indev/lv_indev.c:1781` walks to the first ancestor without that flag, which
is the screen here. The frame's handler never receives the gesture.

**Evidence:** The native UI probe reports `FRAME GESTURE_BUBBLE=1`, receives a
screen gesture, and leaves the page unchanged after a center swipe. Physical
pushers and pager taps are separate paths and are not implicated.

**Use the framework:** Clear gesture bubbling at the intended boundary and
verify that the corresponding release reaches the navigation owner. Retain
release-based page changes and vertical scrolling. Add native tests for a
horizontal swipe, release over the chrome, and a subsequent unrelated tap.

### 2. P2 — Captive-portal discovery omits an upstream compatibility detail

`main/services.cpp:254` redirects non-root GET requests with an empty body.
ESP-IDF's pinned `examples/protocols/http_server/captive_portal/main/main.c:133`
explicitly documents that iOS needs response content for portal detection.
The current implementation can therefore require manually opening the local
address instead of reliably presenting the portal.

**Use the framework example:** Send a short body with the redirect. Evaluate
the same example's DHCP captive-portal advertisement through
`esp_netif_dhcps_option()` (option 114). Validate automatic discovery on actual
iOS and Android phones. This is a source-supported compatibility gap, not a
reproduced failure on the user's phone.

### 3. P2 — The handwritten DNS responder rejects valid additional records

`main/services.cpp:364` requires the DNS packet to end immediately after its one
question. An otherwise valid A query carrying an EDNS OPT record is silently
discarded. This can impede discovery for clients using that query format.

**Reuse opportunity:** The factory's `hal/utils/config_ap/config_ap.cpp:172`
already integrates a `DnsServer` helper. Evaluate its parser and shutdown
contract before adopting it. Alternatively, keep a small bounded adapter and
extend it with valid additional-record handling and malformed-packet fixtures.
Do not copy an example uncritically: the IDF example's DNS stop path deletes its
task without explicitly closing the socket. No replacement helper was validated
as a drop-in during this audit.

### 4. P3 — Display callback outlives the Mooncake application

`main/badge_ui.cpp:98` registers a display event callback that uses global
`app`; that pointer is not cleared when Mooncake destroys the application.
Uninstalling all apps and then rotating the display invokes
`BadgeApp::reflow()` on freed memory.

**Evidence:** AddressSanitizer reproduced the use-after-free in the native UI
probe. The current application stays installed, so this is a teardown/reuse
defect, not an explanation for the previously resolved physical display freeze.

**Use lifecycle ownership:** Register the callback with an owning instance and
remove it through `lv_display_remove_event_cb_with_user_data()` during C++
destruction or explicit UI teardown. An `onClose()`-only fix is insufficient:
pinned Mooncake's `uninstallAllApps()` directly destroys the manager. Exercise
close/reopen, uninstall and display rotation in a regression.

## Additional correctness issues found at framework boundaries

### 5. P2 — Failed IMU reads can preserve an old rotation candidate

`main/board.cpp:286` clears validity but changes `sampledAtMs` only on successful
reads. `main/main.cpp:260` suppresses equal timestamps before checking validity.
A failed poll followed by recovery within the filter's stale interval can keep
settling from the old candidate instead of invalidating it.

Publish a sequence/timestamp for every attempted sample, or check invalidity
before duplicate suppression. Keep the product's settling filter. This is a
source-confirmed path; it was not reproduced on physical hardware in this audit.

### 6. P2 — Automatic rotation can discard a pending touch release

The main loop applies orientation at `main/main.cpp:315` before LVGL processes
input. `main/board.cpp:365` resets LVGL's pointer during rotation. A short contact
between IMU polls can therefore end physically while its release is still
pending in LVGL, allowing rotation to cancel the tap.

Require the framework to consume the physical release before rotating. Simply
moving `lv_timer_handler()` earlier does not guarantee its input timer is due.
Preserve/retry the proposed rotation if the board cannot apply it. Verify short
taps near the settling boundary with real LVGL input. This is an ordering risk
identified from source, not an observed dropped physical tap.

### 7. P2 — Large image allocations can bypass normal error handling

`main/services.cpp:143`, `:158` and `:279` allocate avatar/staging vectors without
handling allocation failure. C++ exceptions are enabled; an uncaught
`std::bad_alloc` can terminate the firmware instead of returning a setup error.
The older profile store explicitly checked image allocation failures.

Use checked ESP-IDF `heap_caps_malloc()` with owned buffers, or contain allocation
failure at the service boundary. Keep decoded JPEG memory under RAII as well,
so a later failed allocation cannot leak it. Test injected allocation failure
while restoring and replacing a profile; the previous saved profile must remain
usable. No actual memory-exhaustion crash was induced during this audit.

### 8. P2 — Dependency verification checks commits, not the compiled contents

`scripts/bootstrap-factory.py:30` verifies `HEAD`, then checks whether expected
patches apply. It does not reject unrelated tracked or untracked source changes
in the cached component directories. A modified dependency can therefore pass
the "dependencies verified" message at the expected commit.

**Evidence:** A disposable local repository with the expected commit and a
modified tracked source file was accepted by the actual `install()` function.
The probe did not modify any real dependency. Result:
`.build/framework-audit/dependency-integrity-probe.json`.

**Use the native manager where practical:** The installed IDF Component Manager
supports Git dependencies with `git`, `version` and `path`, resolved commits and
component hashes. Consolidate ordinary dependencies into its manifest/lock.
Keep required factory patches explicit, either as pinned patched sources or a
checked patch layer. Until then, validate the complete allowed source diff,
unexpected files and submodule state. Preserve the LVGL download optimization
and prove a clean-checkout build before changing the bootstrap path.

## Simplifications worth making

| Area | Current duplication | Recommended direction |
| --- | --- | --- |
| Flash tooling | Native IDF binaries are renamed as Arduino artifacts; upload/discovery still require Arduino CLI, ESP32 core 3.3.10 and a second esptool version. | Use the pinned IDF/esptool toolchain and generated flash metadata inside the existing safety wrapper. Keep USB identity matching, exact partition checks, fresh clock provisioning and readiness checks. Review the generated write set: it includes an `otadata` image. |
| Partition parsing | `scripts/check_flash_layout.py` implements the ESP binary format and checksum. | Consider IDF's `gen_esp32part.PartitionTable.from_binary()` for format parsing. Keep our stricter checksum/padding requirements, exact partition policy and full-sector comparison; the upstream parser alone does not enforce them. |
| Offline worker | `services.cpp:399` wakes the service task every 20 ms with setup inactive. | Use FreeRTOS task notification/event-group waiting while offline; retain bounded active-session deadlines. Battery benefit remains unmeasured. |
| Model updates | Runtime copies model strings/scalars every 100 ms (not avatar pixels); setup rebuilds temporary strings on each application tick. | Update profile data by revision and clock/status text when relevant inputs change. Enabled LVGL observers offer `lv_label_bind_text()` and object-bound observers where helpful. No broad binding rewrite is needed. |
| Styles | Shared widget constructors still install repeated per-object style properties. | Reuse `lv_style_t` with `lv_obj_add_style()` for typography, buttons and cards; preserve explicit circular geometry. This is a consistency/maintenance improvement, not a measured rendering bottleneck. |
| Hardware portability | `main.cpp:263` knows StopWatch's IMU X/Y swap. | Move native-to-display axis mapping into `board.*`, expose display-aligned acceleration, and keep orientation policy hardware-independent. |
| Button debounce | `board.cpp:255` duplicates a small 10 ms filter. | Factory `m5::Button_Class::setRawState()` / `setDebounceThresh()` can provide this, if adopting the helper reduces maintenance. Keep the separate two-button chord policy. This is optional, low-value cleanup. |
| Temporary AP | Default Wi-Fi init allows NVS, then RAM storage is selected. | Follow the factory pattern: set `wifi_init_config_t.nvs_enable = false` before initialization, retaining `WIFI_STORAGE_RAM`. |
| JSON | USB uses ArduinoJson while HTTP uses cJSON. | Consolidate only if strict duplicate-field, trailing-data and embedded-NUL rejection remain intact. Removing one dependency is lower priority than the behavioral fixes. |
| Shared helpers | Active native firmware imports portable policy headers from the historical Arduino directory. | Extract a clearly named shared component without changing serialized formats or behavior. |

The native flasher work is a maintenance simplification, not evidence that the
current verified upload flow wrote incorrect bytes. A raw `idf.py flash` is not
an equivalent replacement for the existing provisioning checks.

## Custom code with a clear reason to remain

| Custom area | Why it remains necessary |
| --- | --- |
| Bounded rotated display flush | Pinned M5GFX's streaming path records incorrect rotated dirty bounds. Our `pushImage` adapter has a regression against the real driver and a physical confirmation. Reverting to the demo's flush reintroduces the freeze. |
| Release/drag/hold tap guard | Plain LVGL/Smooth click binding does not provide the required cancel-after-drag-away-and-back behavior. Native input tests justify this small shared guard. |
| Raw touch adapter | The board supplies controller coordinates; LVGL rotates them once. Coordinate provenance and diagnostic inverse transforms are adapters, not a second calibration system. |
| Orientation settling and button chords | These are badge interaction rules. No matching policy exists in the inspected factory helpers. |
| Circular chrome and page registry | They express product navigation and round-screen geometry. The stock demo's page indicator is application code too. One Mooncake app with modular views is appropriate. |
| Atomic profile record | Its format, SHA verification and rename preserve existing profiles and power-loss behavior. The demo's different FAT layout and autoformat path are incompatible. |
| Clock validation and worker-to-main requests | Checked RTC reads/writes, UTC storage, bounded completion and main-task I2C ownership support truthful phone/USB acknowledgments. The factory RTC read can ignore failures/repair dates, and Smooth's event queue is not a bounded request/response substitute. |
| Absolute HTTP socket deadline | IDF receive timeouts apply per read. `httpd_sess_trigger_close()` queues work on the same potentially blocked HTTP task, so it does not replace the independent absolute deadline. |
| Explicit NVS error handling | The factory Settings wrapper commits in destruction and aborts on many errors. The badge needs checked writes, retry behavior and no automatic erase. Direct IDF NVS calls are already framework usage. |

Replacing every native LVGL object with a Smooth C++ wrapper would mostly change
syntax. Native animation, scrolling and dirty rendering already pass their
focused tests, including zero idle redraws on static pages.

## Suggested implementation order

1. Correct gesture routing and lifecycle cleanup; repair portal redirect/DNS
   compatibility. Verify native swipe/release behavior and actual phone setup.
2. Close the IMU validity/release-ordering gaps and make image allocation failure
   recoverable. Retain the existing physical display and input regressions.
3. Make dependency contents reproducible, then consolidate native flashing before
   preparing the multi-unit production run. Compare the final write set and run
   the existing preservation/clock/readiness checks on the development unit.
4. Extract portable helpers, share styles and reduce idle polling when touching
   those areas. Measure runtime/heap/battery effects before claiming savings.

The first three steps address behavior and repeatability. The fourth is ongoing
maintenance and does not justify another broad framework migration.
