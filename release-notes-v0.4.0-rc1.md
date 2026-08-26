# v0.4.0-rc1

This prerelease adds a complete remote-maintenance path for Windows 98. The
firmware image is byte-for-byte unchanged from v0.3.0-rc1; the new capability
is supplied by `W98AGENT.EXE` and `tools/w98agent-client.py`.

## Highlights

- Execute DOS commands and capture their output remotely.
- Download and upload files through either the Wi-Fi bridge or a direct
  null-modem cable.
- Recover from corrupted frames and missing acknowledgements with W98SER/2
  CRC-protected stop-and-wait transfers.
- Safely reject incomplete or whole-file-CRC-mismatched uploads.
- Use one documented persistent TCP session from connection through `BYE`.

## Hardware acceptance

The exact packaged W98AGENT binary passed on a Compaq Presario 5875 at
19200-8-N-1 through both a direct StarTech/null-modem connection and The Old
Net V4 carrier running this bridge firmware. Acceptance included:

- 2,293-byte command-output transfer over Wi-Fi.
- 55,981-byte Wi-Fi download matching the direct-cable SHA-256 exactly.
- 35,328-byte Wi-Fi upload/download round trip matching the source SHA-256.
- Local recovery from one deliberately corrupted data frame and one deliberately
  lost ACK.

## Safety

W98AGENT is unauthenticated and can run commands and access files. Use it only
on a trusted isolated network or through a separately secured VPN. Never
expose TCP port 23 to the public Internet. End every session with `BYE`.
