# W98SER/2 protocol

W98SER/2 is a small stop-and-wait transport for reliable binary payloads across
an RS-232 link that may electrically reflect host-transmitted bytes. TCP
protects traffic between the modern client and ESP8266; W98SER/2 protects and
paces the UART leg between the ESP8266 carrier and Windows 98.

## Control channel

Control records are ASCII terminated by carriage return. Line feed is ignored.
The handshake response is:

```text
READY<TAB>W98SER/2<TAB>baud
```

Commands are `HELLO`, `PING`, `EXEC<TAB>command`, `GET<TAB>path`,
`PUT<TAB>size<TAB>crc32<TAB>path`, and `BYE`. File or output metadata is
`kind<TAB>size<TAB>crc32<TAB>result`, where `kind` is `FILE` or `OUTPUT`.

W98AGENT V8 and newer also implement an ordered recovery barrier:

```text
SYNC<TAB>token
SYNCED<TAB>token
```

After receiving `READY`, the modern client sends a unique token and ignores
all delayed lines until that exact token returns. Only then may it issue the
first command. This prevents replies queued by an abandoned TCP client from
being mistaken for the new command's response.

## Binary frame

All multibyte integers are little-endian.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic ASCII `W9F2` |
| 4 | 1 | Frame type |
| 5 | 4 | Unsigned sequence number |
| 9 | 2 | Payload length, 0 through 256 |
| 11 | 4 | CRC-32 |
| 15 | variable | Payload |

CRC-32 uses the IEEE polynomial and covers frame type, sequence, length, and
payload; it does not cover the four magic bytes or the CRC field itself.

Frame types are:

- `D`: data
- `A`: acknowledgement
- `N`: negative acknowledgement
- `F`: finish
- `C`: finish confirmation

ACK and NAK frames have no payload. Each data frame must be acknowledged before
the sender advances. A sender retries a frame up to ten times with a five-second
response timeout. The receiver writes only its next expected sequence and ACKs
the immediately preceding sequence without writing it again.

The finish payload contains the complete byte count followed by the complete
payload CRC-32, both unsigned 32-bit values. The receiver ACKs `F` only after
the advertised byte count and CRC agree. The sender then emits `C`; this lets
the receiver leave its duplicate-finish grace period without a fixed transfer
delay.

## Carrier reflection

Some MAX3232 carrier boards, including the tested The Old Net V4, reflect bytes
transmitted by the vintage host back into that host's receive path. W98AGENT
detects this on its initial response and consumes the reflected copy of every
later transmission. Damage to that local copy does not imply that the peer
missed the original frame; delivery is decided by the peer's CRC and ACK.

## Limits

- Maximum individual payload size: 4 GiB minus one byte, imposed by the
  32-bit size field and Windows 98 file APIs.
- Maximum frame payload: 256 bytes.
- One command or transfer is active at a time.
- The control channel and agent are unauthenticated. This protocol provides
  integrity and flow control, not confidentiality or access control.
