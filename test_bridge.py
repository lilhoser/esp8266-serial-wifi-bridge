"""Interactive end-to-end acceptance for the V4 TCP/serial bridge."""

from __future__ import annotations

import socket
import argparse
import sys
import time


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("host", help="bridge IPv4 address or hostname")
    parser.add_argument(
        "--reply", default="SERIAL LINK OK", help="reply expected from terminal"
    )
    parser.add_argument(
        "--echo", action="store_true", help="echo received serial bytes back"
    )
    return parser.parse_args()


def render_for_terminal(chunk: bytes) -> bytes:
    """Give the vintage display normal erase and newline behavior."""
    rendered = bytearray()
    for value in chunk:
        if value in (0x08, 0x7F):
            rendered.extend(b"\b \b")
        elif value == 0x0D:
            rendered.extend(b"\r\n")
        elif value != 0x0A:
            rendered.append(value)
    return bytes(rendered)


def edited_lines(data: bytes) -> list[str]:
    """Apply terminal backspace semantics before evaluating submitted lines."""
    lines: list[str] = []
    line: list[str] = []
    for value in data:
        if value in (0x08, 0x7F):
            if line:
                line.pop()
        elif value == 0x0D:
            lines.append("".join(line).strip().lower())
            line.clear()
        elif value == 0x0A:
            continue
        elif 0x20 <= value <= 0x7E:
            line.append(chr(value))
    return lines


def main() -> int:
    args = parse_args()
    expected = args.reply.lower()
    with socket.create_connection((args.host, 23), timeout=5) as connection:
        connection.settimeout(0.5)
        connection.sendall(
            b"REMOTE LINK READY\r\nTYPE " + args.reply.encode("ascii") +
            b" AND ENTER\r\n"
        )
        print("CONNECTED; waiting for serial-terminal response", flush=True)
        deadline = time.monotonic() + 120
        received = bytearray()
        while time.monotonic() < deadline:
            try:
                chunk = connection.recv(1024)
            except socket.timeout:
                continue
            if not chunk:
                break
            received.extend(chunk)
            print(repr(chunk), flush=True)
            if args.echo:
                connection.sendall(render_for_terminal(chunk))
            if expected in edited_lines(bytes(received)):
                print("PASS: bidirectional bridge confirmed", flush=True)
                return 0
    print(f"FAIL: received {bytes(received)!r}", flush=True)
    return 2


if __name__ == "__main__":
    sys.exit(main())
