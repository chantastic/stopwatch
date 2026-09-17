# Contributing

[chantastic/stopwatch](https://github.com/chantastic/stopwatch) is the source of
truth for the conference badge firmware, browser installer, and website. Make
small, related changes, run the applicable checks, and commit directly to `main`.
Pull requests and another person's approval are not required; do not open a pull
request unless the user asks for one.

## Find the right source

| Area | Source |
| --- | --- |
| Active badge firmware | `firmware/factory_badge/` |
| Browser installer | `web-flasher/` |
| Website and Alto application | `site/` |
| Hardware and release guidance | `docs/` |
| Retained Arduino helpers and regression fixtures | `firmware/devices_badge/`, `tests/` |

Start with [AGENTS.md](AGENTS.md), [the factory stack](docs/factory-stack.md),
[hardware notes](docs/hardware.md), and [the conference clock](docs/conference-clock.md).
The retained connected application and gateway snapshots are historical; the
current badge works offline and uses temporary local Wi-Fi for phone setup.

## Make a change

1. Establish the desired behavior and an example that can be checked.
2. Implement and run the relevant checks below. Record hardware observations
   separately from simulated test results.
3. Commit the tested change to `main` and push to `chantastic/stopwatch`.
4. Flash the connected device when requested, preserving its partition layout
   and user state. Firmware releases and website publication are separate actions;
   a source push performs neither.

Keep passwords, tokens, personal profiles/photos, full-flash backups, NVS dumps,
and raw device logs out of issues, pull requests, and Git. Generated binaries and
test output belong in ignored build directories. The browser installer's
**Copy build details** output provides a small diagnostic summary for bug reports.

## Run checks locally

Use Node.js 24 and the committed npm lockfiles for both web projects:

```sh
cd web-flasher
npm ci --ignore-scripts
npm test
npm run build
```

After changing the browser installer, copy the four generated files from
`.build/web-installer/` into `site/public/stopwatch/install/` and include them in
the same commit. The browser check compares those files byte for byte so the
website cannot silently publish an older installer than the tested source.

From `site/`:

```sh
npm ci --ignore-scripts
npm run check
npm test
npm run build
npm run build:drop
```

The site build verifies and embeds the currently pinned, published firmware
assets. It needs access to those public downloads, but no publishing credentials.
Its Wrangler command uses `--dry-run`; `build:drop` only produces local output.

Firmware host tests currently require macOS, Clang, CMake, Node.js, Python, and
ArduinoJson **7.4.3** headers. They use macOS CommonCrypto and `sips`. Fetch the
pinned factory dependencies and set `ARDUINOJSON_INCLUDE` to the library's `src`
directory, then run from the repository root:

```sh
python3 scripts/bootstrap-factory.py
export ARDUINOJSON_INCLUDE=/path/to/ArduinoJson/src
bash scripts/test.sh
python3 -m unittest discover -s tests -p 'test_package_web_release.py'
```

For native compilation, follow the SDK prerequisites in
[the factory stack](docs/factory-stack.md), then run:

```sh
bash scripts/setup-factory.sh
bash scripts/build.sh
```

Setup verifies ESP-IDF **5.5.4** and the framework revisions in
`firmware/factory_badge/frameworks.json`. Compilation creates local artifacts;
it does not access a badge.

## Automated checks

The workflow defines these four checks. GitHub Actions is currently disabled for
this personal repository, so run the applicable checks locally; committing the
workflow does not enable it or establish a passing run.

| Check name | Coverage |
| --- | --- |
| Browser checks | Installer unit tests, bundled build, and exact website bundle comparison |
| Site checks | Website checks, tests, Worker dry-run build, and local Drop generation |
| Firmware host checks | macOS regression fixtures, native service/clock/UI simulations, and release packaging guards |
| Firmware build | Native ESP32-S3 compilation with the pinned SDK and framework sources |

Workflow actions remain pinned to official commit SHAs. The personal repository
uses no WorkOS-only Socket action or organization secret. Updating a dependency
should include its lockfile or recorded revision and the relevant checks.
Both web projects omit registry-specific `resolved` URLs from their npm lockfiles
using `.npmrc`; versions and integrity hashes remain pinned. Keep that portable
metadata convention without overriding the machine's configured registry.

## Qualify and promote a release

A successful CI run establishes compilation and automated test results. It does
not establish physical touch alignment, a Chrome USB flash, real-phone captive
portal behavior, battery runtime, or RTC retention after battery exhaustion.
Record the applicable physical checks with the build ID and date before
promoting that behavior as verified.

Use [the browser release guide](docs/web-releases.md) and
[the provisioning guide](docs/conference-clock.md) for release preparation.
Preserve saved user state and the exact partition layout. Build once for a
batch, then provision and verify a fresh clock on each badge. A failed preflight
must never trigger an automatic erase or format.

Firmware assets and website publication remain explicit maintainer steps. Verify
the exact build, hashes, website changes, and remaining limitations before making
them available to attendees. The retained WorkOS promotion script does not
publish this repository; see [publication status](docs/site-promotion.md).
