# Remote Windows 98 agent

`W98AGENT.EXE` allows a modern computer to execute DOS commands and transfer
files on a Windows 98 computer through this bridge. The matching
`tools/w98agent-client.py` controller works through either the bridge's TCP
listener or a direct null-modem serial cable.

## Security boundary

The agent is intentionally unauthenticated and can execute commands and read or
write files with the logged-in Windows user's authority. TCP port 23 is raw,
unencrypted TCP. Use the bridge and agent only on a trusted, isolated network or
through a separately secured VPN. Never forward port 23 to the Internet.

Close the agent when the maintenance session is finished. It is not a service
and does not install itself or start automatically.

## End-to-end setup

1. Back up the adapter's original flash if it matters, then flash the release
   image as described in [Flashing](FLASHING.md).
2. Configure Wi-Fi locally at 300-8-N-1 and set a serial rate that the complete
   adapter/cable/computer path has passed. The Old Net V4 reference path was
   validated at 19200 baud.
3. Copy `W98AGENT.EXE` from the release ZIP to the Windows 98 computer. A
   floppy, CD-R, working USB-storage driver, or null-modem connection may be
   used for the initial copy.
4. Install the modern Python environment:

   ```powershell
   .\scripts\install-python-tools.ps1
   ```

5. Connect the Wi-Fi adapter directly to COM1 and ensure it is in RS-232 mode.
6. Start the modern TCP client **before** starting the Windows 98 agent:

   ```powershell
   .\.tools\python-venv\Scripts\python.exe `
     .\tools\w98agent-client.py tcp://192.0.2.10:23 session
   ```

   Replace the documentation address with the address reported by `STATUS`.
7. Wait for `BRIDGE CONNECTED`. On Windows 98, run:

   ```bat
   C:\W98AGENT 19200
   ```

8. After the `W98AGENT V4` banner appears, type `START` in the modern client.
   A successful handshake reports `READY W98SER/2 19200`.
9. Use the session commands below. Finish with `BYE`, which acknowledges the
   request and stops the Windows 98 agent cleanly.

## Session commands

Commands are typed into the modern client, not the Windows 98 console:

```text
EXEC DIR C:\ /A /-P
GET C:\BOOTLOG.TXT .\BOOTLOG.TXT
PUT .\TOOL.EXE C:\TOOL.EXE
BYE
```

- `EXEC` runs `COMMAND.COM /C` on Windows 98, captures stdout and stderr, and
  returns its exit code. The process timeout is five minutes.
- `GET` downloads one file and writes it to the specified modern-host path.
- `PUT` uploads one file. A failed or incomplete upload is deleted rather than
  left under its requested destination name.
- `BYE` stops the agent and closes the TCP session normally.

Paths containing spaces should be quoted. Session operations are sequential;
do not open a second TCP client while the agent is active.

## Direct null-modem operation

The same controller can use a modern USB serial adapter and null-modem cable:

```powershell
.\.tools\python-venv\Scripts\python.exe `
  .\tools\w98agent-client.py COM3 --baud 19200 exec "VER"
```

Start `W98AGENT 19200` first when using a direct cable. The rate passed to both
programs must match. DTR, RTS, hardware flow control, and software flow control
are disabled.

## Reliability and speed

W98SER/2 divides payloads into numbered 256-byte frames. Every frame carries a
CRC-32 and requires an ACK. A CRC error, timeout, or missing ACK causes bounded
retransmission; duplicate frames are acknowledged without being written twice.
A final frame validates the complete byte count and complete-file CRC before an
upload is accepted. Ten consecutive failures abort the transfer.

This flow control replaces fixed inter-block delays. On the validated
19200-baud path, a 55,981-byte file transferred in roughly one minute over
Wi-Fi and about 32 seconds over a direct null-modem cable. Raw 19200-8-N-1 has a
theoretical ceiling near 1,920 bytes per second; Wi-Fi does not change that
last-hop UART limit.

## Troubleshooting

- `BRIDGE CONNECTED` must appear before `W98AGENT` starts. If the order was
  reversed, stop the agent, close the client, and begin again.
- `READY W98SER/2` is required. A `W98SER/1` response identifies an older,
  incompatible agent.
- If the bridge command prompt appears instead of the handshake, the TCP peer
  is not connected. Close the agent and establish TCP first.
- A transfer retry is expected after a damaged frame. Ten consecutive failures
  indicate a dead/mismatched connection and abort safely.
- If Windows suspended while COM1 was open, close the agent and its DOS window,
  then start a fresh session. Do not reuse a terminal that resumed with garbled
  serial state.
- If typing in unrelated programs also skips or duplicates keys, inspect the
  keyboard connector separately; that is not agent protocol traffic.

See [W98SER/2 protocol](W98SER2-PROTOCOL.md) for the wire format.
