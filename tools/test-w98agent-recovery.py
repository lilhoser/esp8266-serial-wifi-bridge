#!/usr/bin/env python3
"""Destructive-to-temp-file recovery test for a running W98AGENT V4."""

import argparse
import importlib.util
import pathlib
import struct
import zlib


CLIENT_PATH = pathlib.Path(__file__).with_name("w98agent-client.py")
SPEC = importlib.util.spec_from_file_location("w98agent_client", CLIENT_PATH)
client_module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(client_module)


def encoded_frame(kind, sequence, payload=b""):
    header = struct.pack("<cIH", kind, sequence, len(payload))
    crc = zlib.crc32(header + payload) & 0xFFFFFFFF
    return client_module.FRAME_MAGIC + header + struct.pack("<I", crc) + payload


def expect_frame(agent, expected_kind, expected_sequence):
    kind, sequence, payload = agent.read_frame()
    if kind != expected_kind or sequence != expected_sequence:
        raise RuntimeError(
            f"expected {expected_kind!r}/{expected_sequence}, "
            f"received {kind!r}/{sequence}"
        )
    return payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("endpoint", help="tcp://address:23 or a serial port")
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--remote", default=r"C:\W98FAULT.BIN")
    args = parser.parse_args()
    payload = bytes((index * 37 + 11) & 0xFF for index in range(4097))
    expected_crc = zlib.crc32(payload) & 0xFFFFFFFF
    agent = client_module.Agent(args.endpoint, args.baud)
    try:
        print("CONNECTED. Start W98AGENT V4, then press Enter here.", flush=True)
        input()
        print(agent.hello(), flush=True)

        agent.send_line(
            f"PUT\t{len(payload)}\t{expected_crc:08X}\t{args.remote}"
        )
        if agent.read_line(20) != "READY":
            raise RuntimeError("agent rejected recovery-test upload")

        first = encoded_frame(client_module.FRAME_DATA, 0,
                              payload[:client_module.FRAME_SIZE])
        damaged = bytearray(first)
        damaged[-1] ^= 0x80
        agent.port.write(damaged)
        agent.port.flush()
        expect_frame(agent, client_module.FRAME_NAK, 0)
        print("PASS: corrupted frame was NAKed", flush=True)

        agent.send_reliable_frame(client_module.FRAME_DATA, 0,
                                  payload[:client_module.FRAME_SIZE])
        agent.send_frame(client_module.FRAME_DATA, 0,
                         payload[:client_module.FRAME_SIZE])
        expect_frame(agent, client_module.FRAME_ACK, 0)
        print("PASS: duplicate frame was ACKed without a second write", flush=True)

        sequence = 1
        for offset in range(client_module.FRAME_SIZE, len(payload),
                            client_module.FRAME_SIZE):
            agent.send_reliable_frame(
                client_module.FRAME_DATA,
                sequence,
                payload[offset:offset + client_module.FRAME_SIZE],
            )
            sequence += 1
        finish = struct.pack("<II", len(payload), expected_crc)
        agent.send_reliable_frame(client_module.FRAME_FIN, sequence, finish)
        agent.send_frame(client_module.FRAME_CONFIRM, sequence)
        stored = agent.read_line(20)
        if stored != f"STORED\t{len(payload)}\t{expected_crc:08X}":
            raise RuntimeError(stored)

        agent.send_line("GET\t" + args.remote)
        metadata = agent.read_line(20).split("\t")
        if (len(metadata) != 4 or metadata[0] != "FILE"
                or int(metadata[1]) != len(payload)
                or int(metadata[2], 16) != expected_crc):
            raise RuntimeError("unexpected download metadata: " + "\t".join(metadata))
        agent.send_line("READY")
        first_retry = expect_frame(agent, client_module.FRAME_DATA, 0)
        agent.send_frame(client_module.FRAME_NAK, 0)
        repeated = expect_frame(agent, client_module.FRAME_DATA, 0)
        if repeated != first_retry:
            raise RuntimeError("retransmitted frame did not match original")
        agent.send_frame(client_module.FRAME_ACK, 0)
        print("PASS: explicit NAK caused an exact retransmission", flush=True)

        received = bytearray(repeated)
        expected_sequence = 1
        while len(received) < len(payload):
            block = expect_frame(agent, client_module.FRAME_DATA,
                                 expected_sequence)
            received.extend(block)
            agent.send_frame(client_module.FRAME_ACK, expected_sequence)
            expected_sequence += 1
        finish_payload = expect_frame(agent, client_module.FRAME_FIN,
                                      expected_sequence)
        if finish_payload != finish:
            raise RuntimeError("finish record did not match upload")
        agent.send_frame(client_module.FRAME_ACK, expected_sequence)
        expect_frame(agent, client_module.FRAME_CONFIRM, expected_sequence)
        if bytes(received) != payload:
            raise RuntimeError("round-trip payload mismatch")
        print("PASS: 4097-byte recovery round trip is exact", flush=True)

        agent.send_line("EXEC\tDEL " + args.remote)
        _, exit_code = agent.receive_payload("OUTPUT")
        if exit_code != 0:
            raise RuntimeError(f"temporary-file cleanup exited {exit_code}")
        agent.send_line("BYE")
        if agent.read_line() != "BYE":
            raise RuntimeError("agent did not acknowledge BYE")
        print("PASS: temporary file removed and agent stopped", flush=True)
        return 0
    finally:
        agent.close()


if __name__ == "__main__":
    raise SystemExit(main())
