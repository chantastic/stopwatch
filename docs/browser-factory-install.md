# Browser first install

The browser installer has a separate, explicit path for converting a factory
M5Stack StopWatch to the conference badge. Desktop Chrome or Edge, a data-capable
USB cable, and the browser's local save-file picker are required. Arduino is not
required. An ordinary update continues to preserve conference settings and
profiles.

**Qualification:** this implementation and its host tests are not evidence of a
physical factory-to-conference conversion. Hardware qualification is pending.
The existing [browser release verification](web-release-verification-2026-09-17.md)
covers updates to an already prepared badge, not this new path. Record the source
revision, browser/OS, factory layout, and observed results separately when testing
on a factory unit.

## Attendee flow

1. Confirm the physical device is an M5Stack StopWatch and choose **First install**.
   The browser connects to the selected USB port and verifies the release,
   ESP32-S3 security state, 16 MiB flash, and recognized factory partition sector.
   Unknown layouts, protected devices, and already prepared badges are refused.
2. Wait while the browser reads all 16 MiB of flash in 256 KiB chunks. Every read
   is checked against the stub's MD5; a second whole-flash device checksum verifies
   the complete backup. No flash has been changed.
3. Choose **Save recovery copy**. A fresh click opens the local save picker.
   The browser writes and closes the file, reads it back, and compares SHA-256.
   Canceling or failing this step prevents installation. A download click alone
   does not count as a verified saved backup.
4. Confirm replacement of the factory firmware and data, then install. The
   browser rechecks the factory sector and whole-flash checksum against the saved
   backup immediately before writing. Keep this same badge connected throughout.
5. After verified writes, the badge restarts. The browser sets the computer's
   current time and UTC offset, verifies that the RTC advances, explicitly
   prepares unavailable profile storage, and checks normal offline operation.
   Only the final successful readiness check means the installation is complete.

The recovery file can contain device settings or other personal data. It is
saved locally, never sent to the website or included in copied issue details.
Keep it private; do not commit or attach it to a public issue. This flow creates
a recovery copy but does not add browser restoration of that copy.

## What changes on the device

`web-flasher/src/factory.js` accepts one exact factory partition sector, including
its checksum and padding, and verifies the complete target map. Additional
factory revisions need their own reviewed allowlist entry; matching a subset of
partition offsets is insufficient.

The plan uses the existing verified `conference-factory-3` bootloader, partition
table, and app0 assets. It writes `0xFF` into these additional ranges:

| Start | Length | Purpose |
| --- | --- | --- |
| `0x9000` | `0x7000` | Clear factory settings and select app0 from erased OTA data |
| `0x310000` | `0xcf0000` | Clear unused app1, profile storage, and crash data |

These regions are written in at most 256 KiB pieces using the pinned library's
compressed write path and per-piece MD5 verification. Chip erase is disabled;
flash image headers are kept unchanged. Do not substitute uncompressed writes:
the pinned library pads their last block beyond some requested boundaries.
The installed target sector and erased OTA selector are read back before reset.

Only explicit first-install provisioning passes `initializeStorage: true` to
the clock protocol. It checks the running build, board/memory, clock, and radio
state before sending the nonce-bound initialization command. The existing
firmware checks all six target partitions and retries a non-destructive mount
before formatting only `ffat`. Already ready storage is left untouched.

## Interrupted installation

- If connecting fails, close other serial applications, check the cable, and
  reconnect. If needed, enter download mode using the
  [manufacturer's StopWatch instructions](https://docs.m5stack.com/en/core/StopWatch).
  Download mode does not itself erase or prepare storage.
- If firmware is installed but final verification did not finish, first try
  **Set clock & check badge**. A lost acknowledgment does not prove a write or
  initialization failed.
- If the conference firmware runs but profile storage still needs preparation,
  choose **Finish first install** and confirm the storage action. This verifies
  the exact target partition sector and an approved boot selector, restarts into
  the pinned firmware, and enables explicit storage initialization. A ready store
  is skipped. Initialization is never retried automatically after an uncertain
  response.
- If firmware no longer boots, retry **Update badge** when the target layout and
  boot selector pass its checks. Unknown or damaged layouts still stop; retain
  the recovery backup and use the project recovery process rather than overriding
  the guards.

## Source and publication

The UI and protocol live in `web-flasher/`; the website contains its generated
bundle. This change does not add a new firmware binary or alter the release pins.
Host coverage includes exact layout checks, bounded writes, backup corruption and
save failures, post-write verification, and explicit initialization behavior.
Physical USB disconnect/recovery and a full factory conversion still need device
qualification. Tested source changes go directly to `chantastic/stopwatch` main.
Website publication is separate; the former WorkOS promotion script cannot
publish this repository. See [publication status](site-promotion.md) for the
pending replacement publisher and unchanged firmware pins.
