# Native services host checks

Run `python3 tests/factory-services/run.py` on macOS. The script uses Clang,
CommonCrypto, Node, and the built-in image converter. Output stays in `.build/`.
It never opens a serial port or contacts the badge.

The fixture compiles the actual validators and record helpers extracted from
`firmware/factory_badge/main/services.cpp`, with host filesystem/crypto adapters.
It writes a synthetic profile/image using the retained Arduino store, reads it
with the native implementation, edits with native code, and reads the result
back with Arduino code. It compares every avatar pixel, checks field retention,
rejects altered hashes/headers, injects failed sync/rename operations, and checks
explicit photo removal. The explicit initialization helper is checked against
all six partition boundaries, an extra partition, active setup, an already
ready store, a recovered mount, and a blank store. Only the last case may format.
Address/undefined-behavior sanitizers are enabled.

The actual native portal browser script is also executed through the existing
clock fixture: automatic sync, fresh timestamps on retry/reopen, UTC offset
direction, nonce, visible failure state, and independent profile saving.

These checks do not emulate ESP-IDF LittleFS mounting, Wi-Fi association, HTTP
socket lifecycle, or physical RTC writes. Integrated builds and device checks
are separate requirements.
