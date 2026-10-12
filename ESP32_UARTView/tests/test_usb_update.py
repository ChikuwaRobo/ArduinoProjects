"""複数機器の誤選択防止と、リセット例外を成功扱いできる条件を検証する。"""
from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import usb_update


class UpdateGuardTest(unittest.TestCase):
    def test_multiple_devices_require_selection(self):
        ports = [SimpleNamespace(device=f"COM{i}", vid=0x303A, pid=0x1001, serial_number=str(i))
                 for i in (37, 38)]
        with patch.object(usb_update.list_ports, "comports", return_value=ports):
            with self.assertRaises(RuntimeError):
                usb_update.find_port()
            self.assertEqual(usb_update.find_port("com38").serial_number, "38")
            self.assertEqual(usb_update.find_port(usb_serial="37").device, "COM37")

    def test_wrong_usb_device_is_rejected(self):
        port = SimpleNamespace(device="COM37", vid=0x1234, pid=0x1001)
        with patch.object(usb_update.list_ports, "comports", return_value=[port]):
            with self.assertRaises(RuntimeError):
                usb_update.find_port("COM37")

    def check_result(self, output, permitted):
        result = SimpleNamespace(stdout=output, returncode=1)
        with patch.object(usb_update.subprocess, "run", return_value=result), redirect_stdout(io.StringIO()):
            if permitted:
                usb_update.esptool("write-flash", allow_reset_disconnect=True)
            else:
                with self.assertRaises(RuntimeError):
                    usb_update.esptool("write-flash", allow_reset_disconnect=True)

    def test_disconnect_only_after_complete_verification(self):
        self.check_result("Hash of data verified.\n" * 4 + "Hard resetting with a watchdog...\nA serial exception error occurred", True)

    def test_partial_flash_failure_is_not_ignored(self):
        self.check_result("Hash of data verified.\n" * 3 + "Hard resetting\nserial exception", False)

    def test_other_error_is_not_ignored(self):
        self.check_result("Hash of data verified.\n" * 4 + "Hard resetting\nFatal write error", False)


if __name__ == "__main__":
    unittest.main()
