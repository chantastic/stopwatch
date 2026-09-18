# Third-party notices

No license has yet been selected for this project's original code. Publishing the repository does not change the separate terms of its dependencies, vendored code, or brand assets.

## Vendored decoder

[`firmware/devices_badge/vendor/stb_image.h`](firmware/devices_badge/vendor/stb_image.h) is stb_image **2.30**, from [nothings/stb](https://github.com/nothings/stb) at revision `2c980bb59875b0d32144a71867fbdebb2f77cd20`.

- License: MIT or public domain, at the recipient's choice, as specified in the header.
- Both upstream license texts and the upstream attribution remain in the vendored header.
- SHA-256: `594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.
- The project's wrapper enables bounded JPEG-only decoding and supplies PSRAM allocation functions; see the [vendor notes](firmware/devices_badge/vendor/README.md).

## Build dependencies

The native factory stack uses ESP-IDF, LVGL, Smooth UI Toolkit, Mooncake,
mooncake_log, M5GFX, M5IOE1, M5PM1, BMI270_BMM150_Sensor, ArduinoJson,
Espressif i2c_bus, and esp_littlefs under their respective upstream licenses.
Exact framework revisions are in `firmware/factory_badge/frameworks.json`;
managed transitive versions are in its `dependencies.lock`.
Factory-derived board and controller code retains M5Stack's MIT attribution in
`firmware/factory_badge/main/vendor/LICENSE-M5Stack` and source headers.
The factory integration patches retain their upstream license via
`firmware/factory_badge/FACTORY-LICENSE`.

ESP32 Arduino, M5Unified, M5GFX, ArduinoJson, and their bundled components retain their upstream licenses. They are installed separately and are not relicensed by this repository.

- [Arduino core for ESP32](https://github.com/espressif/arduino-esp32)
- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

## Badge typography

The native UI's generated `font_mono_*.c` assets derive from Roboto Mono Regular,
Copyright 2015 The Roboto Mono Project Authors. Its `font_sans_*.c` assets derive
from Inter Regular, Copyright 2016 The Inter Project Authors. Both font families
and their converted bitmap subsets use the SIL Open Font License 1.1.

Complete license texts, exact upstream revisions and checksums, and reproducible
conversion instructions are retained in
[`firmware/factory_badge/main/ui/fonts/`](firmware/factory_badge/main/ui/fonts/README.md).
The MIT-licensed LVGL `lv_font_conv` 1.5.3 is used only to generate these assets.

## Brand assets

`init_wordmark.h` contains a raster mask derived from the `init()` wordmark on the [WorkOS conference website](https://workos.com/init). `github_mark.h` contains a raster mask of the official GitHub Invertocat from the [GitHub brand toolkit](https://brand.github.com/foundations/logo). The other provider identifiers and layouts refer to LinkedIn and X.

WorkOS, AuthKit, Pipes, `init()`, M5Stack, LinkedIn, X, GitHub, and their respective marks belong to their owners. Inclusion here is not a trademark license, endorsement, or grant of rights to reuse brand assets. The dependency licenses do not license those marks.

## Public certificate roots

The firmware bundles public server-trust certificates sourced from [Google Trust Services](https://pki.goog/roots.pem), [Let's Encrypt](https://letsencrypt.org/certificates/), and [DigiCert Global Root G2](https://cacerts.digicert.com/DigiCertGlobalRootG2.crt). These are public trust anchors, not private keys or client credentials.
