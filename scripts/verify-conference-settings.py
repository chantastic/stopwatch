#!/usr/bin/env python3
"""Bounded on-device Settings checks through production USB input dispatch.

Requires pyserial, Pillow and zxing-cpp. Refuses a nonempty personal profile;
captures stay under .build. Does not claim physical pusher/touch/IMU testing.
Leaves brightness 50%, orientation Free, radios off and the init() page visible.
"""
import argparse
import importlib.util
import json
import time
from pathlib import Path

from serial.tools import list_ports

spec = importlib.util.spec_from_file_location("conference_usb", Path(__file__).with_name("verify-conference.py"))
usb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(usb)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    out = Path(args.output).resolve()
    build = Path(__file__).resolve().parents[1] / ".build"
    assert build in out.parents, "Private verification output must stay under .build"
    out.mkdir(parents=True, exist_ok=True)
    ports = [p for p in list_ports.comports() if p.device == args.port]
    assert len(ports) == 1 and ports[0].serial_number, "USB identity unavailable"
    identity = (ports[0].serial_number, ports[0].vid, ports[0].pid)
    device = usb.Device(args.port)
    report = {"physical_controls_tested": False, "physical_imu_movement_tested": False}

    def state():
        result = device.status()
        assert result["build"] == "conference-settings-2"
        assert result["page_count"] == 6
        assert not result["name_present"] and not result["configured_mask"] and not result["avatar"]
        return result

    def settled():
        deadline = time.monotonic() + 7
        while time.monotonic() < deadline:
            result = state()
            if not result["preferences_pending"]:
                return result
            time.sleep(.1)
        raise AssertionError("Settings did not persist within bounded wait")

    def reboot():
        nonlocal device
        device.send({"op": "reboot"})
        device.close()
        time.sleep(2)
        matches = [p for p in list_ports.comports()
                   if (p.serial_number, p.vid, p.pid) == identity]
        assert len(matches) == 1, "USB identity changed or became ambiguous"
        device = usb.Device(matches[0].device)
        result = state()
        assert result["page"] == 0 and result["wifi_mode"] == 0 and not result["setup"]
        assert result["bluetooth"] == 0 and result["clock_valid"] and result["store_ready"]
        return result

    def brightness(target):
        result = state()
        for _ in range(10):
            if result["brightness_percent"] == target:
                return result
            result = device.tap(342 if result["brightness_percent"] < target else 126, 167)
        assert result["brightness_percent"] == target
        return result

    def buttons():
        for value in ("blue", "yellow"):
            before = state()
            direction = (-1 if before["rotation"] == 2 else 1) * (1 if value == "blue" else -1)
            after = device.action({"op": "button", "value": value})
            assert after["page"] == (before["page"] + direction) % 6
        device.page(5)

    try:
        initial = state()
        assert not initial["setup"] and initial["wifi_mode"] == 0 and initial["bluetooth"] == 0
        report["initial_status"] = initial
        device.page(5)
        brightness(50)
        settled()
        device.capture(out / "settings-initial.png")

        # Out-and-back gestures must never become taps on any setting.
        before = state()
        for x, y in ((342, 167), (154, 272), (314, 272), (332, 362)):
            device.touch("begin", x, y)
            device.touch("move", x, y - 65)
            device.touch("move", x, y)
            after = device.touch("end", x, y)
            for field in ("page", "brightness_percent", "orientation_mode", "setup"):
                assert after[field] == before[field], (field, before, after)
        report["settings_drag_rejection"] = True

        before = state()
        for x, expected in ((342, 60), (342, 70), (126, 60)):
            result = device.tap(x, 167)
            assert result["brightness_percent"] == expected and result["preferences_pending"], {
                "expected_brightness": expected, "actual_status": result,
                "physical_inputs_before": before["inputs"],
            }
        assert result["preference_writes"] == before["preference_writes"]
        result = settled()
        assert result["preference_writes"] == before["preference_writes"] + 1
        report["brightness_immediate_and_coalesced"] = True

        brightness(10)
        assert device.tap(126, 167)["brightness_percent"] == 10
        before = state()
        pressed = device.touch("begin", 234, 362)
        assert pressed["orientation_mode"] == before["orientation_mode"]
        device.touch("end", 234, 362)
        result = settled()
        assert result["orientation_mode"] == "Default" and result["rotation"] == 0
        device.capture(out / "settings-default-minimum.png")
        result = reboot()
        assert result["brightness_percent"] == 10 and result["orientation_mode"] == "Default" and result["rotation"] == 0
        device.page(5)
        buttons()
        report["default_and_minimum_persist"] = True

        brightness(100)
        assert device.tap(342, 167)["brightness_percent"] == 100
        device.tap(332, 362)
        result = settled()
        assert result["orientation_mode"] == "180°" and result["rotation"] == 2
        result = reboot()
        assert result["brightness_percent"] == 100 and result["orientation_mode"] == "180°" and result["rotation"] == 2
        device.page(5)
        device.capture(out / "settings-opposite-maximum.png")
        buttons()
        report["opposite_and_maximum_persist"] = True

        brightness(50)
        device.tap(136, 362)
        result = settled()
        assert result["orientation_mode"] == "Free"
        device.capture(out / "settings-free.png")
        assert device.tap(154, 272)["setup"]
        result = device.tap(234, 423)
        assert result["page"] == 5 and not result["setup"] and result["wifi_mode"] == 0
        device.page(3)
        assert device.tap(234, 320)["setup"]
        result = device.action({"op": "button", "value": "blue"})
        assert result["page"] == 3 and not result["setup"] and result["wifi_mode"] == 0
        report["setup_returns_to_launch_page_on_cancel"] = True

        result = reboot()
        assert result["orientation_mode"] == "Free" and result["brightness_percent"] == 50
        assert result["network"] == initial["network"] and result["schedule_current"] == -1
        report["free_and_normal_brightness_persist"] = True
        report["final_status"] = result
        report["passed"] = True
        (out / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"passed": True, "output": str(out), "physical_controls_tested": False}))
    finally:
        # Close any AP even after a failed assertion. Never clear profile data.
        try:
            if device.status()["setup"]:
                device.action({"op": "button", "value": "blue"})
        finally:
            device.close()


if __name__ == "__main__":
    main()
