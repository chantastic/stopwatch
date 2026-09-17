"""Exercise the batch flash contract with fake upload/provision executables.

No serial port is opened, and no firmware is built or flashed by these tests.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FlashTests(unittest.TestCase):
    def run_flash(self, provision_exit=0, extra=(), missing=False, preflight_exit=0,
                  resolve_exit=0, mode="artifact"):
        with tempfile.TemporaryDirectory(prefix="conference-flash-") as temporary:
            location = Path(temporary)
            artifacts = location / "artifact with spaces"
            artifacts.mkdir()
            repo = location / "repo"
            (repo / "scripts").mkdir(parents=True)
            (repo / "scripts/flash.sh").write_bytes((ROOT / "scripts/flash.sh").read_bytes())
            (repo / "scripts/flash.sh").chmod(0o755)
            (repo / "scripts/build.sh").write_text('#!/bin/sh\nprintf "[\\"build\\"]\\n" >> "$TEST_FLASH_CALLS"\n')
            (repo / "scripts/build.sh").chmod(0o755)
            if mode != "artifact":
                artifacts = repo / ".build/firmware"
                artifacts.mkdir(parents=True)
            for name in ("devices_badge.ino.bin", "devices_badge.ino.bootloader.bin", "devices_badge.ino.partitions.bin"):
                if not missing:
                    (artifacts / name).write_bytes(b"test artifact, never flashed")
            calls = location / "calls.jsonl"
            fake = location / "fake-tool"
            fake.write_text("#!/usr/bin/env python3\nimport os,sys,json\n"
                            "with open(os.environ['TEST_FLASH_CALLS'],'a') as f: f.write(json.dumps(sys.argv[1:])+'\\n')\n"
                            "if sys.argv[1].endswith('check_flash_layout.py'):\n"
                            " code=int(os.environ['TEST_RESOLVE_EXIT' if '--resolve-port' in sys.argv else 'TEST_PREFLIGHT_EXIT'])\n"
                            " if code: sys.exit(code)\n"
                            " print('/dev/FAKE_STOPWATCH')\n"
                            "if sys.argv[1].endswith('provision-clock.py') and '--help' not in sys.argv: sys.exit(int(os.environ['TEST_PROVISION_EXIT']))\n")
            fake.chmod(0o755)
            environment = {**os.environ, "ARDUINO_CLI": str(fake), "PYTHON": str(fake),
                           "TEST_FLASH_CALLS": str(calls), "TEST_PROVISION_EXIT": str(provision_exit),
                           "TEST_PREFLIGHT_EXIT": str(preflight_exit), "TEST_RESOLVE_EXIT": str(resolve_exit)}
            options = ["--artifact-dir", str(artifacts)] if mode == "artifact" else ["--no-build"] if mode == "no-build" else []
            result = subprocess.run([str(repo / "scripts/flash.sh"), *options,
                                     *extra, "/dev/FAKE_STOPWATCH"], env=environment, capture_output=True, text=True)
            recorded = [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else []
            return result, recorded

    def test_prebuilt_upload_then_mandatory_clock_provision(self):
        result, calls = self.run_flash(extra=("--offset-minutes", "-420"))
        self.assertEqual(result.returncode, 0, result.stderr)
        uploads = [call for call in calls if call[0] == "upload"]
        self.assertEqual(len(uploads), 1)
        upload = uploads[0]
        self.assertIn("PartitionScheme=app3M_fat9M_16MB", upload[2])
        self.assertIn("PSRAM=opi", upload[2])
        self.assertFalse(any("erase" in part for part in upload))
        self.assertFalse(any(call[0] == "compile" for call in calls))
        preflight = next(i for i, call in enumerate(calls) if call[0].endswith("check_flash_layout.py"))
        upload_index = next(i for i, call in enumerate(calls) if call[0] == "upload")
        self.assertLess(preflight, upload_index)
        self.assertTrue(calls[-1][0].endswith("provision-clock.py"))
        self.assertEqual(calls[-1][1:], ["/dev/FAKE_STOPWATCH", "--offset-minutes", "-420"])

    def test_missing_clock_ack_fails_the_whole_batch_unit(self):
        result, calls = self.run_flash(provision_exit=1)
        self.assertEqual(result.returncode, 1)
        self.assertIn("Do not mark this unit ready", result.stderr)
        self.assertTrue(any(call[0] == "upload" for call in calls))

    def test_missing_artifact_fails_before_upload(self):
        result, calls = self.run_flash(missing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing firmware component", result.stderr)
        self.assertFalse(any(call[0] == "upload" for call in calls))

    def test_storage_opt_in_forwarded_only_when_explicit(self):
        result, calls = self.run_flash(extra=("--initialize-profile-storage",))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(calls[-1][1:], ["/dev/FAKE_STOPWATCH", "--initialize-profile-storage"])
        result, calls = self.run_flash()
        self.assertNotIn("--initialize-profile-storage", calls[-1])

    def test_every_build_mode_blocks_upload_on_failed_partition_check(self):
        for mode in ("artifact", "no-build", "build"):
            with self.subTest(mode=mode):
                result, calls = self.run_flash(preflight_exit=1, mode=mode)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(any(call[0].endswith("check_flash_layout.py") for call in calls))
                self.assertFalse(any(call[0] == "upload" for call in calls))
                self.assertFalse(any(call[0].endswith("provision-clock.py") and "--help" not in call for call in calls))

    def test_storage_opt_in_does_not_bypass_partition_check(self):
        result, calls = self.run_flash(preflight_exit=1, extra=("--initialize-profile-storage",))
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(any(call[0] == "upload" for call in calls))

    def test_missing_same_usb_device_after_upload_prevents_provisioning(self):
        result, calls = self.run_flash(resolve_exit=1)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(any(call[0] == "upload" for call in calls))
        self.assertFalse(any(call[0].endswith("provision-clock.py") and "--help" not in call for call in calls))


if __name__ == "__main__":
    unittest.main()
