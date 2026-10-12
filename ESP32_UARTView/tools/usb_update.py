"""USB Serial/JTAG機器を限定して書き込み、起動後のINFO応答を確認する。"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

import serial
from serial.tools import list_ports

PROJECT = Path(__file__).resolve().parent.parent
BUILD = PROJECT / "build"
ESPTOOL_EXE = PROJECT / ".tools" / "esptool-v5.5.0" / "esptool-windows-amd64" / "esptool.exe"


def find_port(requested=None, usb_serial=None):
    # 同じVID/PIDのESP32を複数検出した場合は任意の機器へ書き込まない。
    ports = [p for p in list_ports.comports() if (p.vid, p.pid) == (0x303A, 0x1001)]
    if requested:
        ports = [p for p in ports if p.device.upper() == requested.upper()]
    if usb_serial:
        ports = [p for p in ports if p.serial_number == usb_serial]
    if len(ports) != 1:
        raise RuntimeError(f"Expected one USB Serial/JTAG device, found: {[p.device for p in ports]}. Use --port COMxx.")
    return ports[0]


def info(port):
    # 制御線を切り替えて意図せず再起動させないよう、open前に状態を指定する。
    with serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=1) as connection:
        connection.dtr = False
        connection.rts = False
        connection.port = port
        connection.open()
        connection.reset_input_buffer()
        connection.write(b"\nINFO\n")
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            response = connection.readline().decode("ascii", errors="replace").strip()
            if response.startswith("ESP32_UARTView FW="):
                return response
    raise RuntimeError("No ESP32_UARTView INFO response")


def esptool(*arguments, allow_reset_disconnect=False):
    command = [str(ESPTOOL_EXE)] if ESPTOOL_EXE.is_file() else [sys.executable, "-m", "esptool"]
    result = subprocess.run([*command, *arguments],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, encoding="utf-8", errors="replace",
                            env=os.environ | {"PYTHONIOENCODING": "utf-8", "NO_COLOR": "1"})
    print(result.stdout, end="", flush=True)
    if result.returncode:
        # Windowsではリセットで消えたCOMのclose/設定変更が例外になる場合がある。
        # 4領域の検証とリセット段階まで到達した場合だけ、起動確認へ進める。
        reset_disconnect = (allow_reset_disconnect
                            and result.stdout.count("Hash of data verified.") == 4
                            and "Hard resetting" in result.stdout
                            and "serial exception" in result.stdout)
        if not reset_disconnect:
            raise RuntimeError(f"esptool failed: {result.returncode}")
        print("USB disconnected during reset; checking the application before deciding success.")
    return result.stdout


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--port", help="Select a COM port; otherwise require exactly one native USB device")
    cli.add_argument("--initial", action="store_true", help="Already in manual download mode; use watchdog reset after flash")
    cli.add_argument("--check", action="store_true", help="Read firmware INFO only; do not flash or reset")
    args = cli.parse_args()
    port = find_port(args.port)
    print(f"Target: {port.device}, USB serial: {port.serial_number}", flush=True)
    if args.check:
        print(info(port.device))
        return

    if "5.5.0" not in esptool("version"):
        raise RuntimeError("Use the verified esptool 5.5.0")
    options = json.loads((BUILD / "build.options.json").read_text(encoding="utf-8"))
    fqbn = options["fqbn"]
    for token in ("esp32:esp32:esp32s3", "FlashSize=16M", "PSRAM=opi", "USBMode=hwcdc", "CDCOnBoot=cdc"):
        if token not in fqbn:
            raise RuntimeError(f"Unexpected build configuration: {fqbn}")
    images = [("0x0", BUILD / "ESP32_UARTView.ino.bootloader.bin"),
              ("0x8000", BUILD / "ESP32_UARTView.ino.partitions.bin"),
              ("0xe000", BUILD / "boot_app0.bin"),
              ("0x10000", BUILD / "ESP32_UARTView.ino.bin")]
    for _, path in images:
        if not path.is_file():
            raise RuntimeError(f"Missing build image: {path}. Run tools/update.ps1 to build first.")
    config = (PROJECT / "Config.h").read_text(encoding="utf-8")
    expected_version = re.search(r'firmwareVersion\s*=\s*"([^"]+)"', config).group(1)

    # 初回は既にROM起動済み。通常はUSBの制御要求で自動的にROMへ移行。
    before = "no-reset" if args.initial else "usb-reset"
    probe = esptool("--chip", "esp32s3", "--port", port.device,
                    "--before", before, "--after", "no-reset", "flash-id")
    if "ESP32-S3" not in probe or "Detected flash size: 16MB" not in probe:
        raise RuntimeError("Target chip/flash does not match ESP32-S3 with 16MB flash")
    flash_args = ["--chip", "esp32s3", "--port", port.device, "--baud", "460800",
                  "--before", "no-reset", "--after", "watchdog-reset" if args.initial else "hard-reset",
                  "write-flash", "--flash-mode", "keep", "--flash-freq", "keep", "--flash-size", "keep"]
    for address, path in images:
        flash_args.extend([address, str(path)])
    esptool(*flash_args, allow_reset_disconnect=True)

    # USB再列挙を待ち、同一USBシリアルの機器のFW応答を確認する。
    deadline = time.monotonic() + 20
    last_error = None
    while time.monotonic() < deadline:
        time.sleep(0.5)
        try:
            running = find_port(usb_serial=port.serial_number)
            response = info(running.device)
            fields = dict(re.findall(r"([A-Z]+)=([^ ]+)", response))
            if fields.get("FW") != expected_version or fields.get("FLASH") != "16777216" or fields.get("PSRAM") != "8388608":
                raise RuntimeError(f"Firmware or memory mismatch: {response}")
            print(f"Boot verified on {running.device}: {response}")
            return
        except (RuntimeError, serial.SerialException, OSError) as error:
            last_error = error
    raise RuntimeError(f"Flash completed, but application boot was not verified: {last_error}")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    try:
        main()
    except (RuntimeError, OSError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
