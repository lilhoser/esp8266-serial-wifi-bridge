#!/usr/bin/env python3
"""Keyboard-independent whole-line test for echo-free bridge firmware."""

import argparse
import sys
import time

import serial


PROMPT = b"SERIALWIFI> "
COMMAND = b"AT\r"
REPLY = b"\r\nOK\r\n" + PROMPT


def read_until_prompt(port, timeout):
    result = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        chunk = port.read(256)
        if chunk:
            result.extend(chunk)
            if result.endswith(PROMPT):
                break
    return bytes(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="serial port, for example COM6 or /dev/ttyUSB0")
    parser.add_argument("--rounds", type=int, default=20)
    args = parser.parse_args()

    port = serial.Serial(port=None, baudrate=300, bytesize=8, parity="N",
                         stopbits=1, timeout=0.1, write_timeout=3)
    port.port = args.port
    port.dtr = False
    port.rts = False
    port.open()
    try:
        startup = read_until_prompt(port, 35)
        if not startup.endswith(PROMPT):
            print(f"STARTUP FAIL: prompt not received; received {startup!r}",
                  flush=True)
            return 1
        print("STARTUP PASS: prompt received after Wi-Fi result", flush=True)

        # USB-UART adapters normally return only REPLY. Some RS-232 carriers
        # also reflect the exact outgoing line before the firmware response.
        accepted = (REPLY, COMMAND + REPLY)
        for round_number in range(1, args.rounds + 1):
            port.reset_input_buffer()
            port.write(COMMAND)
            port.flush()
            response = read_until_prompt(port, 8)
            if response not in accepted:
                print(f"FAIL: round {round_number} response mismatch", flush=True)
                print(f"EXPECTED: {REPLY!r} (optionally preceded by {COMMAND!r})",
                      flush=True)
                print(f"RECEIVED: {response!r}", flush=True)
                return 1
            print(f"Round {round_number:02d}/{args.rounds:02d} passed", flush=True)

        print(f"PASS - {args.rounds} whole-line AT/OK rounds over {args.port}", flush=True)
        return 0
    finally:
        port.close()


if __name__ == "__main__":
    sys.exit(main())
