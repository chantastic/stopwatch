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

These checks do not emulate ESP-IDF LittleFS mounting, Wi-Fi association, HTTP
socket lifecycle, or physical RTC writes. Integrated builds and device checks
are separate requirements.
