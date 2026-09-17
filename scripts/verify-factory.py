#!/usr/bin/env python3
"""Check native badge USB dispatch/rendering, without claiming physical alignment.

Requires pyserial, Pillow and zxing-cpp. Refuses personal profile captures;
restores original brightness/orientation/network and leaves Touch test open.
"""
import argparse
import hashlib
import importlib.util
import json
import secrets
import time
from pathlib import Path
from PIL import Image, ImageDraw
import zxingcpp

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("legacy_usb", ROOT / "scripts/verify-conference.py")
usb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(usb)

class Device(usb.Device):
    def action(self, obj):
        # Boot emits an unsolicited status. Correlate each command so a queued
        # boot/earlier reply cannot masquerade as the next navigation result.
        nonce = secrets.token_hex(8)
        self.send({**obj, "nonce": nonce})
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline:
            state = json.loads(self.response(b"CONFERENCE_STATUS ", deadline - time.monotonic()))
            if state.get("nonce") == nonce:
                return state
        raise RuntimeError("No matching command acknowledgment")

    def status(self):
        return self.action({"op": "status"})

    def capture(self, path):
        self.send({"op": "capture_badge"})
        width, height = map(int, self.response(b"BADGE_CAPTURE ").split())
        assert (width, height) in ((468, 466), (466, 468))
        total = width * height * 3
        data = bytearray()
        deadline = time.monotonic() + 8
        while len(data) < total and time.monotonic() < deadline:
            data.extend(self.serial.read(total - len(data)))
        assert len(data) == total, "Incomplete native framebuffer"
        self.response(b"BADGE_CAPTURE_END", 3)
        image = Image.frombytes("RGB", (width, height), bytes(data))
        image.save(path)
        return image

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    out = Path(args.output).resolve()
    assert ROOT / ".build" in out.parents, "Captures must remain private under .build"
    out.mkdir(parents=True, exist_ok=True)
    device = Device(args.port)
    original = device.status()
    assert original["build"] == "conference-factory-3"
    assert not original["configured_mask"] and not original["avatar"] and not original["name_present"]
    firmware = ROOT / ".build/firmware/devices_badge.ino.bin"
    report = {"initial": original, "physical_alignment_tested": False,
              "local_firmware_sha256": hashlib.sha256(firmware.read_bytes()).hexdigest()}

    def wait_state(predicate, timeout=6):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = device.status()
            if predicate(state): return state
            time.sleep(.1)
        raise AssertionError("Expected state not reached: " + json.dumps(state))

    def settle(): return wait_state(lambda s: not s["preferences_pending"])
    def tap(x, y):
        device.touch("begin", x, y)
        time.sleep(.09)
        device.touch("end", x, y)
        time.sleep(.12)
        return device.status()
    def set_orientation(name):
        tap({"Free": 136, "Default": 234, "180°": 332}[name], 369)
        return wait_state(lambda s: s["orientation_mode"] == name and (name == "Free" or s["rotation"] == (0 if name == "Default" else 2)))
    def set_brightness(value):
        for _ in range(10):
            state = device.status()
            if state["brightness_percent"] == value: return
            tap(342 if state["brightness_percent"] < value else 126, 177)
        raise AssertionError("Brightness did not reach target")

    try:
        assert original["clock_valid"] and original["store_ready"] and original["wifi_mode"] == 0
        images = []
        for index in range(6):
            device.page(index); time.sleep(.2)
            picture = device.capture(out / f"page-{index}.png")
            images.append(picture)
            if index == 4:
                mask = Image.new("L", picture.size)
                ImageDraw.Draw(mask).ellipse((2, 1, 466, 465), fill=255)
                circular = Image.new("RGB", picture.size)
                circular.paste(picture, mask=mask)
                assert [v.text for v in zxingcpp.read_barcodes(circular)] == ["https://drop.workos.cloud/stopwatch"]
        report["pages_and_hack_qr"] = True
        device.page(0); before = device.capture(out / "animation-a.png")
        time.sleep(.4); after = device.capture(out / "animation-b.png")
        assert before.tobytes() != after.tobytes()
        report["animation"] = True
        device.page(5)
        before = device.status()
        for x, y in ((342, 177), (154, 283), (314, 283), (332, 369)):
            device.touch("begin", x, y)
            device.touch("move", x, y - 70)
            device.touch("move", x, y)
            device.touch("end", x, y)
            time.sleep(.15)
            current = device.status()
            assert all(current[k] == before[k] for k in ("page", "brightness_percent", "orientation_mode", "setup")), "Drag became a tap"
        report["drag_rejection"] = True
        set_brightness(60 if original["brightness_percent"] != 60 else 50)
        settle()
        set_brightness(original["brightness_percent"])
        for name, rotation in (("Default", 0), ("180°", 2)):
            set_orientation(name); settle()
            tap(314, 283)
            device.send({"op": "touch_test_status"})
            state = json.loads(device.response(b"TOUCH_TEST_STATUS "))
            assert state["active"] and state["rotation"] == rotation
            tap(234, 234)
            device.send({"op": "touch_test_status"})
            state = json.loads(device.response(b"TOUCH_TEST_STATUS "))
            assert state["touch_model"] == "factory-native" and (state["x"], state["y"]) == (234, 234)
            assert not state["sensor"], "Idle polling relabeled a simulated point as physical"
            device.capture(out / f"touch-{rotation}.png")
            device.action({"op": "button", "value": "blue"})
        report["settings_and_simulated_rotated_touch"] = True
        set_orientation(original["orientation_mode"]); settle()
        tap(154, 283)
        active = wait_state(lambda s: s["setup"] and s["wifi_mode"] == 2)
        report["ap_started"] = active["wifi_mode"] == 2
        device.send({"op": "capture_badge"})
        device.response(b"CAPTURE_REJECTED")
        device.action({"op": "button", "value": "yellow"})
        closed = wait_state(lambda s: not s["setup"] and s["wifi_mode"] == 0)
        assert closed["page"] == 5
        report["ap_cancel_and_private_capture_guard"] = True
        # Restart validates saved preferences and an independently restored RTC.
        device.send({"op": "reboot"}); device.close(); time.sleep(2)
        device = Device(args.port)
        restored = wait_state(lambda s: s["page"] == 0 and s["clock_valid"])
        for key in ("brightness_percent", "orientation_mode", "network", "configured_mask", "avatar", "store_ready"):
            assert restored[key] == original[key], key
        report["restart_restoration"] = True
        device.page(5); tap(314, 283)
        device.send({"op": "touch_test_status"})
        report["final_touch_test"] = json.loads(device.response(b"TOUCH_TEST_STATUS "))
        assert report["final_touch_test"]["active"]
        report["final"] = device.status(); report["passed"] = True
        (out / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({"passed": True, "physical_alignment_tested": False, "output": str(out)}))
    finally:
        if not report.get("passed"):
            (out / "failure.json").write_text(json.dumps(report, indent=2) + "\n")
            # A failed check must still close setup and release any test contact.
            try:
                device.send({"op": "touch", "phase": "end", "x": 234, "y": 234})
                device.action({"op": "button", "value": "yellow"})
                device.page(5)
                set_brightness(original["brightness_percent"])
                set_orientation(original["orientation_mode"])
                settle()
            except Exception:
                pass
        device.close()

if __name__ == "__main__": main()
