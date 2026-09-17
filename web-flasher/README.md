# StopWatch browser installer

Source for the desktop Chrome / Edge installer at
<https://drops.workos.cloud/stopwatch>. It uses Web Serial and pinned
`esptool-js` 0.6.1. Arduino is not required to install from the browser.

## Installation paths

- **Update badge:** requires the conference partition map and an approved app0
  boot selector. It preserves saved settings and profiles.
- **First install:** accepts only the recognized M5Stack factory layout. It
  reads and verifies a complete 16 MiB recovery backup, requires saving and
  verifying that file locally, then requires explicit confirmation before
  replacing factory firmware and storage.
- **Finish first install:** explicitly prepares unavailable profile storage on
  the verified conference layout. It leaves ready storage alone. Use this after
  an interrupted first install; it is not a general reset button.
- **Set clock & check badge:** sets this computer's time and checks firmware,
  storage, the advancing hardware clock, and offline operation. It never formats
  storage.

First install also requires the browser's local save-file picker. The recovery
file may contain personal device data; it stays on the user's computer and must
not be uploaded with an issue report. See
[browser first install](../docs/browser-factory-install.md) for safeguards,
recovery, and the remaining hardware qualification.

## Development

Use Node.js 24, matching CI:

```sh
cd web-flasher
npm ci --ignore-scripts
npm test
npm run build
```

The bundle is written to `.build/web-installer/` at the repository root. Copy its
contents into `site/public/stopwatch/install/` when updating the website; CI
checks that this committed copy matches the browser source. Firmware binaries,
backup files, and generated release content never belong in Git.

`src/installer.js` coordinates the UI and USB session; `src/factory.js` owns the
factory-layout allowlist, backup verification, and bounded write plan;
`src/guards.js` verifies releases and device reads; `src/protocol.js` owns clock,
readiness, and explicit storage initialization. The underlying flashing library
continues to own serial framing, compression, flash writes, and MD5 verification.

The first-install change reuses the existing `conference-factory-3` release;
it changes neither firmware binaries nor release pins. Host tests use synthetic
devices and do not establish a successful physical factory conversion. Publish
only through the [reviewed Alto promotion workflow](../docs/site-promotion.md).
