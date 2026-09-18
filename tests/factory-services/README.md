# Native services host checks

Run `python3 tests/factory-services/run.py` on macOS. The script uses Clang,
CommonCrypto, Node, and the built-in image converter. Output stays in `.build/`.
It never opens a serial port or contacts the badge.

The fixture compiles the actual validators and record helpers extracted from
`firmware/factory_badge/main/services.cpp`, with host filesystem/crypto adapters.
It writes a synthetic profile/image using the retained Arduino store, reads it
with the native implementation, edits with native code, and reads the result
back with Arduino code. Records without a company retain version 1; populated
companies use version 2 with one bounded field appended. Tests read both versions,
preserve all prior fields and every avatar pixel, reject truncated/extra/invalid
metadata and altered hashes/headers, inject failed sync/rename operations, and
check explicit company/photo removal. Version 2 requires current firmware;
clearing company writes the version 1 format again. Reads never rewrite records.

The actual native form validator and page renderer cover optional company,
older requests retaining the saved value, explicit clearing, duplicate/unknown
fields, type checks, 60-codepoint/120-byte UTF-8 bounds, HTML escaping and template
token isolation. cJSON tree access is adapted; this does not exercise its parser.
The explicit initialization helper is checked against
all six partition boundaries, an extra partition, active setup, an already
ready store, a recovered mount, and a blank store. Only the last case may format.
Address/undefined-behavior sanitizers are enabled.

The actual native portal browser script is also executed through the existing
clock fixture: automatic sync, fresh timestamps on retry/reopen, UTC offset
direction, nonce, visible failure state, and independent profile saving. A second
browser fixture checks company submission/clearing and retained edits after a
failed save, through the actual form submit handler.

`browser-photo.cjs` executes the actual photo script with native-API adapters and
synthetic file objects. It covers local square preview before any upload, source
URL lifetime, FileReader and JPEG data-URL fallbacks, null/wrong-type canvas
results, bounded resizing, oversized/empty/unsupported files, failed/stalled
decode/encode/upload/save, stale selection cancellation, explicit removal,
failed/timed-out Cancel retaining ongoing preparation, confirmed Cancel discarding
late/completed previews, upload-token validation and keeping draft edits after failure. An uncertain Save
is never automatically retried. These adapters check the accepted HEIC code path;
they do not decode a real HEIC or emulate the iOS photo picker.

An optional real-browser check uses an already-installed Playwright and Chrome:

```sh
node tests/factory-services/browser-photo-native.cjs
```

`PLAYWRIGHT_MODULE` may point to an existing Playwright module, and `CHROME_PATH`
may select an existing Chrome executable. The check installs nothing. It starts
a loopback-only HTTP fixture and an isolated browser, creates a synthetic PNG,
and uses real Image/FileReader/canvas/JPEG APIs. It verifies the crop's pixels,
JPEG dimensions/type/size, upload Content-Length, `/image` then `/save` ordering,
native conversion fallbacks, malformed-image failure and explicit photo discard
preserving the draft. Chrome **153.0.8010.50** passed on September 17, 2026.
This is desktop Chrome evidence; actual iPhone captive-window picking, HEIC
decoding, iCloud-only photos and phone-to-badge transfer remain separate checks.

These checks do not emulate ESP-IDF LittleFS mounting, Wi-Fi association, HTTP
socket lifecycle, or physical RTC writes. Integrated builds and device checks
are separate requirements.

The native reset fixture executes the production queue and worker operation against
synthetic profile/image records. Acceptance does not change the profile; only the
verified empty-record commit reports success. It checks setup/staging/service/store
exclusion, duplicate requests, execution-time rechecking, failed open/sync/rename
retaining every prior field/photo, explicit retry, durable empty-record readback,
revision changes, and no automatic re-execution after terminal results. A separate
legacy-file sentinel and the format-call counter remain unchanged. These checks
never request a reset from a connected badge.
