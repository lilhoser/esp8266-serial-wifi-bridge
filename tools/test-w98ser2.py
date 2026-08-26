#!/usr/bin/env python3
"""Deterministic W98SER/2 transport tests with corruption and ACK loss."""

import importlib.util
import pathlib
import threading
import time
import zlib


MODULE_PATH = pathlib.Path(__file__).with_name("w98agent-client.py")
SPEC = importlib.util.spec_from_file_location("serial_agent", MODULE_PATH)
serial_agent = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(serial_agent)
serial_agent.FRAME_TIMEOUT = 0.1


class MemoryPort:
    def __init__(self):
        self.peer = None
        self.buffer = bytearray()
        self.lock = threading.Lock()
        self.corrupt_first_data = False
        self.drop_first_ack = False

    def read(self, length):
        deadline = time.monotonic() + 0.01
        while time.monotonic() < deadline:
            with self.lock:
                if self.buffer:
                    take = min(length, len(self.buffer))
                    result = bytes(self.buffer[:take])
                    del self.buffer[:take]
                    return result
            time.sleep(0.0005)
        return b""

    def write(self, data):
        data = bytes(data)
        if data.startswith(serial_agent.FRAME_MAGIC) and len(data) >= 15:
            kind = data[4:5]
            if kind == serial_agent.FRAME_DATA and self.corrupt_first_data:
                self.corrupt_first_data = False
                damaged = bytearray(data)
                damaged[-1] ^= 0x80
                data = bytes(damaged)
            elif kind == serial_agent.FRAME_ACK and self.drop_first_ack:
                self.drop_first_ack = False
                return len(data)
        with self.peer.lock:
            self.peer.buffer.extend(data)
        return len(data)

    def flush(self):
        return None

    def close(self):
        return None


def make_agent(port):
    agent = serial_agent.Agent.__new__(serial_agent.Agent)
    agent.port = port
    agent.pending = bytearray()
    return agent


def test_recovery():
    sender_port = MemoryPort()
    receiver_port = MemoryPort()
    sender_port.peer = receiver_port
    receiver_port.peer = sender_port
    sender_port.corrupt_first_data = True
    receiver_port.drop_first_ack = True
    sender = make_agent(sender_port)
    receiver = make_agent(receiver_port)
    payload = bytes((index * 37 + 11) & 0xFF for index in range(4097))
    expected_crc = zlib.crc32(payload) & 0xFFFFFFFF
    result = {}

    def receive():
        result["payload"] = receiver.receive_frames(len(payload), expected_crc)

    thread = threading.Thread(target=receive)
    thread.start()
    sequence = 0
    for offset in range(0, len(payload), serial_agent.FRAME_SIZE):
        sender.send_reliable_frame(
            serial_agent.FRAME_DATA,
            sequence,
            payload[offset:offset + serial_agent.FRAME_SIZE],
        )
        sequence += 1
    finish = len(payload).to_bytes(4, "little") + expected_crc.to_bytes(4, "little")
    sender.send_reliable_frame(serial_agent.FRAME_FIN, sequence, finish)
    sender.send_frame(serial_agent.FRAME_CONFIRM, sequence)
    thread.join(3)
    assert not thread.is_alive(), "receiver did not finish"
    assert result["payload"] == payload


if __name__ == "__main__":
    test_recovery()
    print("PASS: recovered from one corrupted data frame and one lost ACK")
