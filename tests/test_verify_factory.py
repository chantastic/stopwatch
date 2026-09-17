"""Exercise native capture navigation without opening a serial port."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import MagicMock, patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("verify_factory", ROOT / "scripts/verify-factory.py")
factory = importlib.util.module_from_spec(spec)
# Hardware/image dependencies are optional on CI. Only the capture/navigation
# contract is exercised here; these fakes do not claim framebuffer/QR coverage.
with patch.dict("sys.modules", {name: MagicMock() for name in ("serial", "PIL", "zxingcpp")}):
    spec.loader.exec_module(factory)


class FakeDevice:
    def __init__(self, status):
        self.state = status
        self.calls = []

    def status(self):
        self.calls.append(("status",))
        return self.state

    def page(self, target):
        if target == 2 and self.state.get("after_dark_unlocked") is False:
            raise AssertionError("Attempted to navigate to a locked page")
        self.calls.append(("page", target))

    def capture(self, path):
        self.calls.append(("capture", path.name))
        return SimpleNamespace(size=(468, 466))


class CapturePagesTests(unittest.TestCase):
    def capture(self, state):
        device = FakeDevice(state)
        with patch.object(factory.time, "sleep"), patch.object(
                factory.zxingcpp, "read_barcodes",
                return_value=[SimpleNamespace(text="https://drop.workos.cloud/stopwatch")]):
            captured, skipped = factory.capture_pages(device, Path("unused-private-output"))
        # Page navigation and framebuffer capture are the only mutations: never
        # provision a clock or enter Morse merely to make a capture reachable.
        self.assertEqual([call[1] for call in device.calls if call[0] == "page"], captured)
        self.assertEqual(len([call for call in device.calls if call[0] == "capture"]), len(captured))
        self.assertEqual(device.calls[4], ("status",))
        return captured, skipped

    def test_locked_invitation_is_skipped_and_reported(self):
        captured, skipped = self.capture({"after_dark_unlocked": False, "page_count": 5})
        self.assertEqual(captured, [0, 1, 3, 4, 5])
        self.assertEqual(skipped, [{"page": 2, "reason": "After Dark is locked"}])

    def test_currently_unlocked_invitation_is_captured(self):
        captured, skipped = self.capture({"after_dark_unlocked": True, "page_count": 6})
        self.assertEqual(captured, [0, 1, 2, 3, 4, 5])
        self.assertEqual(skipped, [])

    def test_published_firmware_without_reveal_flag_retains_six_pages(self):
        captured, skipped = self.capture({"page_count": 6})
        self.assertEqual(captured, [0, 1, 2, 3, 4, 5])
        self.assertEqual(skipped, [])


if __name__ == "__main__":
    unittest.main()
