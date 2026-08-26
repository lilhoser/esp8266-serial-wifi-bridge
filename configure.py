"""Configure the serial Wi-Fi bridge without exposing its Wi-Fi password."""

from __future__ import annotations

import ipaddress
import argparse
import re
import sys
import time
import tkinter as tk
from typing import Any


BAUD = 300


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True, help="serial port, for example COM6")
    parser.add_argument("--ssid", required=True, help="Wi-Fi network name")
    return parser.parse_args()


def masked_password(prompt: str) -> str:
    root = tk.Tk()
    root.title("Configure Serial Wi-Fi Bridge")
    root.resizable(False, False)
    result = {"value": ""}

    tk.Label(root, text=prompt, padx=16, pady=10).pack()
    entry = tk.Entry(root, show="*", width=44)
    entry.pack(padx=16, pady=(0, 10))

    def submit() -> None:
        result["value"] = entry.get()
        root.destroy()

    def cancel() -> None:
        root.destroy()

    buttons = tk.Frame(root)
    buttons.pack(pady=(0, 14))
    tk.Button(buttons, text="Configure", command=submit, width=12).pack(
        side=tk.LEFT, padx=5
    )
    tk.Button(buttons, text="Cancel", command=cancel, width=12).pack(
        side=tk.LEFT, padx=5
    )
    root.bind("<Return>", lambda _event: submit())
    root.protocol("WM_DELETE_WINDOW", cancel)
    entry.focus_set()
    root.eval("tk::PlaceWindow . center")
    root.mainloop()
    return result["value"]


def read_until(port: Any, marker: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = port.read(256)
        if chunk:
            data.extend(chunk)
            if marker in data:
                return bytes(data)
    safe = bytes(data).decode("ascii", "backslashreplace")
    raise RuntimeError(f"Timed out waiting for {marker!r}; received {safe!r}")


def send_line(port: Any, value: str) -> None:
    port.write(value.encode("ascii") + b"\r")
    port.flush()


def main() -> int:
    args = parse_args()
    try:
        import serial
    except ImportError:
        print("pyserial is required; install dependencies from requirements.txt.")
        return 2
    password = masked_password(
        f"Paste the password for {args.ssid}. It will not be displayed or stored:"
    )
    if not (8 <= len(password) <= 63):
        print("Cancelled or invalid password length; adapter was not changed.")
        return 2
    if "\r" in password or "\n" in password:
        print("Password cannot contain carriage return or line feed.")
        return 2
    try:
        password.encode("ascii")
    except UnicodeEncodeError:
        print("This bridge accepts an ASCII Wi-Fi password only.")
        return 2

    port = serial.Serial()
    port.port = args.port
    port.baudrate = BAUD
    port.timeout = 0.1
    port.xonxoff = False
    port.rtscts = False
    port.dsrdtr = False
    port.dtr = False
    port.rts = False
    port.open()
    try:
        # Opening USB normally resets the ESP8266. Startup now resolves Wi-Fi
        # success/failure before exposing the command prompt.
        read_until(port, b"SERIALWIFI> ", 35)
        send_line(port, "AT")
        response = read_until(port, b"SERIALWIFI> ", 8)
        if b"OK" not in response:
            raise RuntimeError("Bridge did not acknowledge AT")
        print(f"{args.port} V4 bridge ready at 300 baud.")

        send_line(port, "WIFI")
        read_until(port, b"SSID: ", 8)
        send_line(port, args.ssid)
        read_until(port, b"USE THIS SSID (Y/N)? ", 8)
        send_line(port, "Y")
        read_until(port, b"PASSWORD (HIDDEN): ", 8)
        send_line(port, password)
        read_until(port, b"RE-ENTER PASSWORD (HIDDEN): ", 8)
        send_line(port, password)
        read_until(port, b"SAVE AND CONNECT (Y/N)? ", 10)
        password = ""  # Drop the local reference before association output.
        send_line(port, "Y")
        response = read_until(port, b"TCP 23 READY", 70)

        match = re.search(rb"WIFI CONNECTED - IP ([0-9.]+)", response)
        if not match:
            raise RuntimeError("Association completed without a reported IP")
        address = ipaddress.ip_address(match.group(1).decode("ascii"))
        if not address.is_private:
            raise RuntimeError(f"Unexpected non-private address: {address}")
        print(f"Wi-Fi connected: {address}")
        print("TCP port 23 ready. Password was not displayed or stored locally.")
        return 0
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
