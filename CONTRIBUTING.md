# Contributing

This repository is the review home for the conference badge firmware, browser
installer, and website. Use an issue to describe a bug or desired experience,
then submit a pull request for review. Small, related changes are easier to test
and promote together.

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

## Request and review a change

1. Describe the current behavior, desired result, and an example someone can
   reproduce. Use the bug or change issue form when it fits.
2. Work on a branch and open a pull request against `main`. Link the request and
   explain the final behavior, relevant tests, and any release work still needed.
3. Have another team member review the change and wait for the required checks.
   Record hardware observations separately from simulated test results.
4. Merge the reviewed change. Promotion to the live badge release or website is
   a separate maintainer action; CI does not publish or flash devices.

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
the same pull request. Browser checks compares those files byte for byte so the
website cannot silently publish an older installer than the reviewed source.

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

## Required CI checks

All four checks run on pull requests and pushes to `main`:

| Check name | Coverage |
| --- | --- |
| Browser checks | Installer unit tests, bundled build, and exact website bundle comparison |
| Site checks | Website checks, tests, Worker dry-run build, and local Drop generation |
| Firmware host checks | macOS regression fixtures, native service/clock/UI simulations, and release packaging guards |
| Firmware build | Native ESP32-S3 compilation with the pinned SDK and framework sources |

Keep these check names stable when changing the workflow, because branch
protection uses them. Workflow actions are pinned to reviewed official commit
SHAs. Updating a dependency should include its lockfile or recorded revision and
the relevant checks.

Both npm jobs use WorkOS's pinned
[Socket Firewall setup action](https://github.com/workos/setup-socket-firewall)
and the existing private-repository `SOCKET_FIREWALL_TOKEN` organization secret.
The token is passed only to that action; it is not a publishing credential. Setup
fails before dependency installation if the organization secret is unavailable.
Resolve secret delivery through the established WorkOS process instead of
switching CI to an unprotected registry. GitHub's separate Socket project report
does not replace this dependency-download control.
Both web projects omit registry-specific `resolved` URLs from their npm lockfiles
using `.npmrc`; versions and integrity hashes remain pinned. Keep that portable
metadata convention without overriding the machine's protected registry.

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

Firmware assets, installer/site changes, and Alto promotion remain explicit
maintainer steps. Review the exact build, hashes, website changes, and remaining
limitations before making them available to attendees.
