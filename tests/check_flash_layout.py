#!/usr/bin/env python3
"""Read-only partition compatibility and USB identity checks; no devices used."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("flash_layout", ROOT / "scripts/check_flash_layout.py")
layout = importlib.util.module_from_spec(spec)
spec.loader.exec_module(layout)


def table(entries):
    data = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, start, size,
                                name.encode("ascii"), flags)
                    for kind, subtype, start, size, name, flags in entries)
    data += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(data).digest()
    return data.ljust(0xC00, b"\xff")


def port(address="/dev/cu.test", serial="fixture-01"):
    return {"address": address, "protocol": "serial",
            "properties": {"serialNumber": serial, "vid": "0x303A", "pid": "0x1001"}}


class LayoutChecks(unittest.TestCase):
    def test_established_table_matches_with_sector_padding(self):
        compiled = table(layout.EXPECTED_PARTITIONS)
        self.assertEqual(layout.validate_layout(compiled, compiled.ljust(0x1000, b"\xff")),
                         hashlib.sha256(compiled.ljust(0x1000, b"\xff")).hexdigest())

    def test_factory_table_is_blocked(self):
        factory = [(1, 2, 0x9000, 0x4000, "nvs", 0),
                   (1, 0, 0xD000, 0x2000, "otadata", 0),
                   (1, 1, 0xF000, 0x1000, "phy_init", 0),
                   (0, 0x10, 0x20000, 0x4F0000, "ota_0", 0),
                   (0, 0x11, 0x510000, 0x4F0000, "ota_1", 0),
                   (1, 0x81, 0xA00000, 0x400000, "storage", 0),
                   (1, 3, 0xE00000, 0x10000, "coredump", 0)]
        with self.assertRaisesRegex(RuntimeError, "differs"):
            layout.validate_layout(table(layout.EXPECTED_PARTITIONS), table(factory).ljust(0x1000, b"\xff"))

    def test_corrupt_blank_truncated_and_trailing_data_are_blocked(self):
        compiled = table(layout.EXPECTED_PARTITIONS)
        corrupt = bytearray(compiled.ljust(0x1000, b"\xff"))
        corrupt[10] ^= 1
        for data in (b"\xff" * 0x1000, bytes(corrupt), compiled,
                     compiled.ljust(0xFFF, b"\xff") + b"\x00"):
            with self.assertRaises(RuntimeError):
                layout.validate_layout(compiled, data)

    def test_different_build_table_never_authorizes_migration(self):
        changed = list(layout.EXPECTED_PARTITIONS)
        changed[0] = (1, 2, 0x9000, 0x4000, "nvs", 0)
        data = table(changed)
        with self.assertRaisesRegex(RuntimeError, "Compiled firmware"):
            layout.validate_layout(data, data.ljust(0x1000, b"\xff"))

    def test_usb_identity_follows_renumbered_port(self):
        identity = layout.selected_identity([port()], "/dev/cu.test")
        with patch.object(layout, "discovered_ports", return_value=[port("/dev/cu.changed")]):
            self.assertEqual(layout.resolve_identity("fake-cli", identity), "/dev/cu.changed")

    def test_duplicate_or_missing_identity_is_blocked(self):
        for ports in ([port(serial="")], [port(), port("/dev/cu.other")], []):
            with self.assertRaises(RuntimeError):
                layout.selected_identity(ports, "/dev/cu.test")

    def test_same_address_with_different_device_does_not_match(self):
        identity = layout.selected_identity([port()], "/dev/cu.test")
        with patch.object(layout, "discovered_ports", return_value=[port(serial="other-device")]), \
             patch.object(layout.time, "monotonic", side_effect=[0, 1, 21]), \
             patch.object(layout.time, "sleep"):
            with self.assertRaisesRegex(RuntimeError, "did not return"):
                layout.resolve_identity("fake-cli", identity)

    def test_preflight_only_runs_read_flash_and_writes_no_identity_on_mismatch(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            compiled = folder / "compiled.bin"
            compiled.write_bytes(table(layout.EXPECTED_PARTITIONS))
            args = argparse.Namespace(compiled=compiled, arduino_cli="fake", fqbn="pinned",
                                      sketch=folder, port="/dev/cu.test", output_dir=folder / "output",
                                      state_file=folder / "state.json")
            commands = []

            def fake_read(command, **kwargs):
                commands.append(command)
                Path(command[-1]).write_bytes(b"\xff" * 4096)
                return subprocess.CompletedProcess(command, 0, b"private hardware metadata", b"")

            with patch.object(layout, "discover_esptool", return_value=Path("fake-esptool")), \
                 patch.object(layout, "discovered_ports", return_value=[port()]), \
                 patch.object(layout.subprocess, "run", side_effect=fake_read):
                with self.assertRaises(RuntimeError):
                    layout.run_preflight(args)
            self.assertEqual(len(commands), 1)
            self.assertIn("read-flash", commands[0])
            self.assertEqual(commands[0][-3:-1], ["0x8000", "0x1000"])
            self.assertNotIn("write-flash", commands[0])
            self.assertNotIn("erase-flash", commands[0])
            self.assertFalse(args.state_file.exists())


if __name__ == "__main__":
    unittest.main()
