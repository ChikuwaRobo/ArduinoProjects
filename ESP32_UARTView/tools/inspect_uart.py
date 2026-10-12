"""USB経由でUART受信と描画元の行を観測し、必要なら内部ループバックを確認する。"""
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import sys
import time

import serial
from usb_update import find_port


def request(connection, command):
    connection.reset_input_buffer()
    connection.write(("\n" + command + "\n").encode("ascii"))
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        response = connection.readline().decode("ascii", errors="replace").strip()
        if command == "INFO" and response.startswith("ESP32_UARTView FW="):
            return response
        if command == "SELFTEST" and response.startswith("SELFTEST "):
            if response != "SELFTEST SENT_INTERNAL":
                raise RuntimeError(response)
            return response
        if command == "SNAP" and response.startswith("SNAP "):
            body = connection.readline().decode("ascii", errors="replace").rstrip("\r\n")
            if not body.startswith("TEXT "):
                raise RuntimeError("Incomplete snapshot response")
            return {"header": response, "text": body[5:]}
        if command == "PULSE" and response.startswith("PULSE "):
            body = connection.readline().decode("ascii", errors="replace").strip()
            return {"header": response, "intervals": body}
    raise RuntimeError(f"No response to {command}")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--port")
    cli.add_argument("--samples", type=int, default=5)
    cli.add_argument("--interval", type=float, default=2)
    cli.add_argument("--output", type=Path)
    cli.add_argument("--self-test", action="store_true", help="Run UART hardware loopback; GPIO TX stays disabled")
    cli.add_argument("--pulse", action="store_true", help="Sample RX edge intervals without driving GPIO TX")
    args = cli.parse_args()
    if args.samples < 1 or args.interval < 0:
        cli.error("samples must be positive and interval nonnegative")
    port = find_port(args.port)
    records = []
    with serial.Serial(port=None, baudrate=115200, timeout=0.5, write_timeout=1) as connection:
        connection.dtr = connection.rts = False
        connection.port = port.device
        connection.open()
        if args.self_test:
            print(request(connection, "SELFTEST"), flush=True)
            time.sleep(0.2)
        for i in range(args.samples):
            response = request(connection, "INFO")
            snapshot = request(connection, "SNAP")
            record = {"time": datetime.now().astimezone().isoformat(), "port": port.device,
                      "internal_self_test": args.self_test,
                      "info": response, "fields": dict(re.findall(r"([A-Z]+)=([^ ]+)", response)),
                      "snapshot": snapshot}
            if args.pulse:
                record["pulse"] = request(connection, "PULSE")
                print(record["pulse"], flush=True)
            records.append(record)
            print(response, flush=True)
            print(snapshot["header"], flush=True)
            print(snapshot["text"][:240], flush=True)
            if i + 1 < args.samples:
                time.sleep(args.interval)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    try:
        main()
    except (RuntimeError, OSError, serial.SerialException) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
