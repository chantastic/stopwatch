import importlib.util
import json
import os
from pathlib import Path
import pty
import unittest

spec = importlib.util.spec_from_file_location("provision_clock", Path(__file__).resolve().parents[1] / "scripts/provision-clock.py")
clock = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clock)


class Time:
    def __init__(self):
        self.value = 1789560000.2

    def now(self):
        return self.value

    def sleep(self, duration):
        self.value += duration


class FakePort:
    def __init__(self, timer, mutate=None):
        self.timer = timer
        self.requests = []
        self.lines = []
        self.mutate = mutate
        self.store_ready = True

    def write(self, raw):
        request = json.loads(raw)
        self.requests.append(request)
        reply = {"protocol": 1, "ok": True, "valid": True, "nonce": request["nonce"],
                 "source": "computer", "epoch": int(self.timer.now()),
                 "rtc_epoch": int(self.timer.now()), "offset_minutes": -420}
        if request["op"] == "status":
            reply = {"nonce": request["nonce"], "build": clock.EXPECTED_BUILD,
                     "board": 30, "flash_bytes": 16777216, "psram_bytes": 8388608,
                     "store_ready": self.store_ready, "clock_valid": True, "rtc": True,
                     "setup": False, "wifi_mode": 0, "bluetooth": 0}
        if request["op"] == "initialize_conference_storage":
            self.store_ready = True
            reply = {"nonce": request["nonce"], "ok": True, "store_ready": True}
        if self.mutate:
            reply = self.mutate(request, reply)
        if reply is not None:
            prefix = {"clock_set": b"CLOCK_ACK ", "clock_status": b"CLOCK_STATUS ",
                      "status": b"CONFERENCE_STATUS ",
                      "initialize_conference_storage": b"STORAGE_ACK "}[request["op"]]
            self.lines.extend([b"ignored unrelated diagnostic", prefix + b"{bad json",
                               prefix + json.dumps({**reply, "nonce": "deadbeef"}).encode(),
                               prefix + json.dumps(reply).encode()])

    def readline(self, timeout):
        if self.lines:
            return self.lines.pop(0)
        self.timer.sleep(timeout)
        return b""


class ProvisionTests(unittest.TestCase):
    def run_clock(self, mutate=None, **kwargs):
        timer = Time()
        port = FakePort(timer, mutate)
        result = clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep,
                                 timeout=5, offset=-420, **kwargs)
        return result, port

    def test_fresh_ack_followed_by_advancing_hardware_clock(self):
        result, port = self.run_clock()
        self.assertTrue(result["verified"])
        self.assertEqual([r["op"] for r in port.requests], ["clock_set", "clock_status", "status"])
        self.assertTrue(result["unit_ready"])
        self.assertNotEqual(port.requests[0]["nonce"], port.requests[1]["nonce"])

    def test_missing_ack_is_failure(self):
        with self.assertRaisesRegex(clock.ClockError, "No fresh CLOCK_ACK"):
            self.run_clock(lambda request, reply: None)

    def test_retry_uses_new_epoch_and_nonce(self):
        attempts = []

        def drop_first(request, reply):
            attempts.append(request)
            return None if len(attempts) == 1 else reply

        self.run_clock(drop_first)
        self.assertGreater(attempts[1]["epoch"], attempts[0]["epoch"])
        self.assertNotEqual(attempts[0]["nonce"], attempts[1]["nonce"])

    def test_wrong_timezone_rejected(self):
        with self.assertRaisesRegex(clock.ClockError, "display offset"):
            self.run_clock(lambda req, reply: {**reply, "offset_minutes": 420})

    def test_old_epoch_rejected(self):
        with self.assertRaisesRegex(clock.ClockError, "current computer time"):
            self.run_clock(lambda req, reply: {**reply, "epoch": reply["epoch"] - 10})

    def test_stopped_rtc_rejected(self):
        with self.assertRaisesRegex(clock.ClockError, "did not advance"):
            self.run_clock(lambda req, reply: {**reply, "rtc_epoch": 1789560000})

    def test_missing_status_rejected(self):
        with self.assertRaisesRegex(clock.ClockError, "No fresh CLOCK_STATUS"):
            self.run_clock(lambda req, reply: None if req["op"] == "clock_status" else reply)

    def test_missing_readiness_rejected(self):
        with self.assertRaisesRegex(clock.ClockError, "No fresh CONFERENCE_STATUS"):
            self.run_clock(lambda req, reply: None if req["op"] == "status" else reply)

    def test_factory_unformatted_storage_never_counts_as_ready(self):
        with self.assertRaisesRegex(clock.ClockError, "explicit storage provisioning"):
            self.run_clock(lambda req, reply: {**reply, "store_ready": False} if req["op"] == "status" else reply)

    def test_default_never_initializes_storage(self):
        timer = Time()
        port = FakePort(timer)
        port.store_ready = False
        with self.assertRaisesRegex(clock.ClockError, "explicit storage provisioning"):
            clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep, offset=-420)
        self.assertNotIn("initialize_conference_storage", [req["op"] for req in port.requests])

    def test_opt_in_does_not_initialize_an_already_ready_store(self):
        result, port = self.run_clock(initialize_profile_storage=True)
        self.assertFalse(result["storage_initialized"])
        self.assertNotIn("initialize_conference_storage", [req["op"] for req in port.requests])

    def test_explicit_factory_initialization_requires_ack_and_new_status(self):
        timer = Time()
        port = FakePort(timer)
        port.store_ready = False
        result = clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep,
                                 offset=-420, initialize_profile_storage=True)
        self.assertTrue(result["storage_initialized"])
        commands = [req["op"] for req in port.requests]
        self.assertEqual(commands[-3:], ["status", "initialize_conference_storage", "status"])
        request = port.requests[-2]
        self.assertEqual(request["confirm"], "ERASE_FFAT_FOR_CONFERENCE")
        self.assertNotEqual(request["nonce"], port.requests[-1]["nonce"])

    def test_opt_in_cannot_format_wrong_hardware(self):
        timer = Time()
        port = FakePort(timer, lambda req, reply: {**reply, "board": 1} if req["op"] == "status" else reply)
        port.store_ready = False
        with self.assertRaisesRegex(clock.ClockError, "Wrong board"):
            clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep,
                            offset=-420, initialize_profile_storage=True)
        self.assertNotIn("initialize_conference_storage", [req["op"] for req in port.requests])

    def test_failed_storage_ack_cannot_count_as_ready(self):
        timer = Time()
        port = FakePort(timer, lambda req, reply: {**reply, "ok": False} if req["op"] == "initialize_conference_storage" else reply)
        port.store_ready = False
        with self.assertRaisesRegex(clock.ClockError, "initialization was not verified"):
            clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep,
                            offset=-420, initialize_profile_storage=True)

    def test_radio_hardware_and_build_readiness_invariants(self):
        failures = (("build", "legacy-connected"), ("board", 1), ("flash_bytes", 4194304),
                    ("psram_bytes", 0), ("wifi_mode", 2), ("bluetooth", 1),
                    ("setup", True), ("rtc", False), ("clock_valid", False),
                    ("board", 30.0), ("wifi_mode", False), ("store_ready", 1))
        for field, value in failures:
            with self.subTest(field=field), self.assertRaises(clock.ClockError):
                self.run_clock(lambda req, reply: {**reply, field: value} if req["op"] == "status" else reply)

    def test_wrong_types_rejected(self):
        for field, value in (("protocol", True), ("ok", 1), ("epoch", "1789560000"), ("offset_minutes", -420.0)):
            with self.subTest(field=field), self.assertRaises(clock.ClockError):
                self.run_clock(lambda req, reply: {**reply, field: value})

    def test_arbitrary_error_payload_never_echoed(self):
        with self.assertRaisesRegex(clock.ClockError, "invalid acknowledgment") as caught:
            self.run_clock(lambda req, reply: {**reply, "ok": False, "error": "PRIVATE_PAYLOAD"})
        self.assertNotIn("PRIVATE_PAYLOAD", str(caught.exception))

    def test_bad_computer_clock_never_written(self):
        timer = Time()
        timer.value = 0
        port = FakePort(timer)
        with self.assertRaisesRegex(clock.ClockError, "Computer time"):
            clock.provision(port, now=timer.now, monotonic=timer.now, sleep=timer.sleep)
        self.assertEqual(port.requests, [])

    def test_posix_transport_handles_split_and_multiple_lines(self):
        master, slave = pty.openpty()
        port = clock.SerialPort(os.ttyname(slave))
        try:
            os.write(master, b"first\nsecond")
            self.assertEqual(port.readline(1), b"first")
            self.assertEqual(port.readline(0.01), b"")
            os.write(master, b" half\nthird\n")
            self.assertEqual(port.readline(1), b"second half")
            self.assertEqual(port.readline(1), b"third")
            port.write(b"request\n")
            self.assertEqual(os.read(master, 100), b"request\n")
        finally:
            port.close()
            os.close(master)
            os.close(slave)

    def test_posix_transport_discards_oversized_lines(self):
        master, slave = pty.openpty()
        port = clock.SerialPort(os.ttyname(slave))
        try:
            for _ in range(6):
                os.write(master, b"a" * 500)
                self.assertEqual(port.readline(0.01), b"")
            os.write(master, b"bad tail\nvalid\n")
            self.assertEqual(port.readline(1), b"valid")
        finally:
            port.close()
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    unittest.main()
