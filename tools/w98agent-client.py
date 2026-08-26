#!/usr/bin/env python3
"""Controller for W98AGENT.EXE over null-modem serial or a TCP bridge."""

import argparse
import pathlib
import shlex
import socket
import struct
import sys
import time
import urllib.parse
import zlib

import serial


SUPPORTED_BAUDS = (300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)
FRAME_MAGIC = b"W9F2"
FRAME_DATA = b"D"
FRAME_ACK = b"A"
FRAME_NAK = b"N"
FRAME_FIN = b"F"
FRAME_CONFIRM = b"C"
FRAME_SIZE = 256
FRAME_RETRIES = 10
FRAME_TIMEOUT = 5.0


class SocketPort:
    def __init__(self, endpoint):
        parsed = urllib.parse.urlparse(endpoint)
        if parsed.scheme != "tcp" or not parsed.hostname or not parsed.port:
            raise ValueError("TCP endpoint must look like tcp://host:port")
        self.socket = socket.create_connection((parsed.hostname, parsed.port), 10)
        self.socket.settimeout(0.2)

    def read(self, length):
        try:
            return self.socket.recv(length)
        except socket.timeout:
            return b""

    def write(self, data):
        self.socket.sendall(data)
        return len(data)

    def flush(self):
        return None

    def reset_input_buffer(self):
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            try:
                if not self.socket.recv(4096):
                    break
            except socket.timeout:
                break

    def close(self):
        self.socket.close()


class Agent:
    def __init__(self, port_name, baudrate=115200):
        if port_name.lower().startswith("tcp://"):
            self.port = SocketPort(port_name)
        else:
            self.port = serial.Serial(port=None, baudrate=baudrate, bytesize=8,
                                      parity="N", stopbits=1, timeout=0.2,
                                      write_timeout=60)
            self.port.port = port_name
            self.port.dtr = False
            self.port.rts = False
            self.port.open()
        self.pending = bytearray()

    def close(self):
        self.port.close()

    def send_line(self, value):
        self.port.write(value.encode("ascii") + b"\r")
        self.port.flush()

    def _read_exact_raw(self, length, timeout):
        data = bytearray()
        if self.pending:
            take = min(length, len(self.pending))
            data.extend(self.pending[:take])
            del self.pending[:take]
        deadline = time.monotonic() + timeout
        while len(data) < length and time.monotonic() < deadline:
            chunk = self.port.read(length - len(data))
            if chunk:
                data.extend(chunk)
        if len(data) != length:
            raise TimeoutError(f"expected {length} bytes, received {len(data)}")
        return bytes(data)

    def send_frame(self, kind, sequence, payload=b""):
        if len(kind) != 1 or len(payload) > FRAME_SIZE:
            raise ValueError("invalid frame")
        header = struct.pack("<cIH", kind, sequence, len(payload))
        crc = zlib.crc32(header + payload) & 0xFFFFFFFF
        self.port.write(FRAME_MAGIC + header + struct.pack("<I", crc) + payload)
        self.port.flush()

    def read_frame(self, timeout=FRAME_TIMEOUT):
        matched = 0
        deadline = time.monotonic() + timeout
        while matched != len(FRAME_MAGIC) and time.monotonic() < deadline:
            value = self._read_exact_raw(1, max(0.01, deadline - time.monotonic()))
            if value[0] == FRAME_MAGIC[matched]:
                matched += 1
            else:
                matched = 1 if value[0] == FRAME_MAGIC[0] else 0
        if matched != len(FRAME_MAGIC):
            raise TimeoutError("frame sync timed out")
        header = self._read_exact_raw(11, FRAME_TIMEOUT)
        kind, sequence, length = struct.unpack("<cIH", header[:7])
        expected_crc = struct.unpack("<I", header[7:])[0]
        if length > FRAME_SIZE:
            raise ValueError(f"invalid frame length {length}")
        payload = self._read_exact_raw(length, FRAME_TIMEOUT)
        actual_crc = zlib.crc32(header[:7] + payload) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError("frame CRC mismatch")
        return kind, sequence, payload

    def send_reliable_frame(self, kind, sequence, payload=b""):
        for _ in range(FRAME_RETRIES):
            self.send_frame(kind, sequence, payload)
            try:
                reply_kind, reply_sequence, _ = self.read_frame()
            except (TimeoutError, ValueError):
                continue
            if reply_sequence == sequence and reply_kind == FRAME_ACK:
                return
            if reply_sequence == sequence and reply_kind == FRAME_NAK:
                continue
        raise TimeoutError(f"frame {sequence} was not acknowledged")

    def receive_frames(self, size, expected_crc):
        data = bytearray()
        expected_sequence = 0
        failures = 0
        next_report = 1024
        while len(data) < size:
            try:
                kind, sequence, payload = self.read_frame()
            except (TimeoutError, ValueError):
                self.send_frame(FRAME_NAK, expected_sequence)
                failures += 1
                if failures >= FRAME_RETRIES:
                    raise TimeoutError("payload transfer exceeded retry limit")
                continue
            if (kind == FRAME_DATA and sequence == expected_sequence and payload
                    and len(data) + len(payload) <= size):
                data.extend(payload)
                self.send_frame(FRAME_ACK, sequence)
                expected_sequence += 1
                failures = 0
                if size >= 1024 and len(data) >= next_report:
                    print(f"receiving {len(data)}/{size} bytes", flush=True)
                    next_report = ((len(data) // 1024) + 1) * 1024
            elif (kind == FRAME_DATA and expected_sequence
                  and sequence == expected_sequence - 1):
                self.send_frame(FRAME_ACK, sequence)
            else:
                self.send_frame(FRAME_NAK, expected_sequence)

        actual_crc = zlib.crc32(data) & 0xFFFFFFFF
        for _ in range(FRAME_RETRIES):
            try:
                kind, sequence, payload = self.read_frame()
            except (TimeoutError, ValueError):
                self.send_frame(FRAME_NAK, expected_sequence)
                continue
            valid_finish = (
                kind == FRAME_FIN
                and sequence == expected_sequence
                and len(payload) == 8
                and struct.unpack("<II", payload) == (size, actual_crc)
                and actual_crc == expected_crc
            )
            if not valid_finish:
                self.send_frame(FRAME_NAK, expected_sequence)
                continue
            self.send_frame(FRAME_ACK, sequence)
            finish_deadline = time.monotonic() + 2.0
            while time.monotonic() < finish_deadline:
                try:
                    final_kind, final_sequence, _ = self.read_frame(0.25)
                except (TimeoutError, ValueError):
                    continue
                if final_kind == FRAME_CONFIRM and final_sequence == sequence:
                    return bytes(data)
                if final_kind == FRAME_FIN and final_sequence == sequence:
                    self.send_frame(FRAME_ACK, sequence)
            return bytes(data)
        raise RuntimeError("transfer finish was not validated")

    def read_line(self, timeout=10):
        data = bytearray()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.pending:
                value = bytes([self.pending.pop(0)])
            else:
                value = self.port.read(1)
            if not value:
                continue
            if value == b"\n":
                continue
            if value == b"\r":
                following = self.port.read(1)
                if following and following != b"\n":
                    self.pending.extend(following)
                if data:
                    return data.decode("ascii", "replace")
                continue
            data.extend(value)
        raise TimeoutError("agent response timed out")

    def read_exact(self, length, timeout=300):
        data = bytearray()
        next_report = 1024
        if self.pending:
            take = min(length, len(self.pending))
            data.extend(self.pending[:take])
            del self.pending[:take]
        deadline = time.monotonic() + timeout
        while len(data) < length and time.monotonic() < deadline:
            chunk = self.port.read(min(65536, length - len(data)))
            if chunk:
                data.extend(chunk)
                if length >= 1024 and len(data) >= next_report:
                    print(f"receiving {len(data)}/{length} bytes", flush=True)
                    next_report = ((len(data) // 1024) + 1) * 1024
        if len(data) != length:
            raise TimeoutError(f"expected {length} bytes, received {len(data)}")
        return bytes(data)

    def hello(self):
        self.port.reset_input_buffer()
        self.send_line("HELLO")
        response = self.read_line()
        if not response.startswith("READY\tW98SER/2\t"):
            raise RuntimeError(response)
        return response

    def receive_payload(self, expected_kind):
        header = self.read_line(20)
        fields = header.split("\t")
        if fields[0] == "ERR":
            raise RuntimeError(header)
        if len(fields) != 4 or fields[0] != expected_kind:
            raise RuntimeError(f"unexpected header: {header}")
        size = int(fields[1])
        expected_crc = int(fields[2], 16)
        result_code = int(fields[3])
        self.send_line("READY")
        data = self.receive_frames(size, expected_crc)
        return data, result_code

    def execute(self, command):
        self.send_line("EXEC\t" + command)
        return self.receive_payload("OUTPUT")

    def get(self, remote_path):
        self.send_line("GET\t" + remote_path)
        return self.receive_payload("FILE")[0]

    def put(self, local_path, remote_path):
        data = pathlib.Path(local_path).read_bytes()
        crc = zlib.crc32(data) & 0xFFFFFFFF
        self.send_line(f"PUT\t{len(data)}\t{crc:08X}\t{remote_path}")
        response = self.read_line(20)
        if response != "READY":
            raise RuntimeError(response)
        sequence = 0
        for offset in range(0, len(data), FRAME_SIZE):
            self.send_reliable_frame(
                FRAME_DATA, sequence, data[offset:offset + FRAME_SIZE]
            )
            sequence += 1
        finish = struct.pack("<II", len(data), crc)
        self.send_reliable_frame(FRAME_FIN, sequence, finish)
        self.send_frame(FRAME_CONFIRM, sequence)
        done = self.read_line(300)
        expected = f"STORED\t{len(data)}\t{crc:08X}"
        if done != expected:
            raise RuntimeError(done)
        return len(data), crc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "port", help="serial port such as COM3, or tcp://address:23"
    )
    parser.add_argument("--baud", type=int, choices=SUPPORTED_BAUDS,
                        default=115200)
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("ping")
    sub.add_parser("bye")
    execute = sub.add_parser("exec")
    execute.add_argument("command")
    get = sub.add_parser("get")
    get.add_argument("remote_path")
    get.add_argument("local_path")
    put = sub.add_parser("put")
    put.add_argument("local_path")
    put.add_argument("remote_path")
    sub.add_parser(
        "session",
        help="keep one TCP connection open; type START after W98AGENT begins",
    )
    args = parser.parse_args()

    agent = Agent(args.port, args.baud)
    try:
        if args.action == "session":
            print("BRIDGE CONNECTED. Start W98AGENT, then type START here.",
                  flush=True)
            if sys.stdin.readline().strip().upper() != "START":
                raise RuntimeError("session cancelled before START")
            print(agent.hello(), flush=True)
            print("Commands: EXEC command | GET remote local | PUT local remote | BYE",
                  flush=True)
            while True:
                request = sys.stdin.readline()
                if not request:
                    raise RuntimeError("session input closed without BYE")
                fields = shlex.split(request, posix=False)
                if not fields:
                    continue
                action = fields[0].upper()
                if action == "EXEC" and len(fields) >= 2:
                    command = request.strip()[len(fields[0]):].lstrip()
                    data, result = agent.execute(command)
                    print(data.decode("ascii", "replace"), end="")
                    if data and not data.endswith(b"\n"):
                        print()
                    print(f"[exit {result}]", flush=True)
                elif action == "GET" and len(fields) == 3:
                    data = agent.get(fields[1])
                    pathlib.Path(fields[2]).write_bytes(data)
                    print(f"received {len(data)} bytes", flush=True)
                elif action == "PUT" and len(fields) == 3:
                    size, crc = agent.put(fields[1], fields[2])
                    print(f"sent {size} bytes, CRC32 {crc:08X}", flush=True)
                elif action == "BYE" and len(fields) == 1:
                    agent.send_line("BYE")
                    if agent.read_line() != "BYE":
                        raise RuntimeError("agent did not acknowledge BYE")
                    print("agent stopped", flush=True)
                    return 0
                else:
                    print("Invalid session command", flush=True)

        print(agent.hello())
        if args.action == "ping":
            return 0
        if args.action == "bye":
            agent.send_line("BYE")
            response = agent.read_line()
            if response != "BYE":
                raise RuntimeError(response)
            print("agent stopped")
            return 0
        if args.action == "exec":
            data, result = agent.execute(args.command)
            sys.stdout.buffer.write(data)
            if data and not data.endswith(b"\n"):
                print()
            print(f"[exit {result}]")
            return result
        if args.action == "get":
            data = agent.get(args.remote_path)
            pathlib.Path(args.local_path).write_bytes(data)
            print(f"received {len(data)} bytes")
            return 0
        if args.action == "put":
            size, crc = agent.put(args.local_path, args.remote_path)
            print(f"sent {size} bytes, CRC32 {crc:08X}")
            return 0
    finally:
        agent.close()


if __name__ == "__main__":
    sys.exit(main())
