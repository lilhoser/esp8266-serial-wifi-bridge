# v0.4.0-rc2

This prerelease hardens long-running Windows 98 maintenance sessions. Firmware
build 15 keeps TCP bridge traffic out of the local command parser after a peer
disconnects, while W98AGENT V9 and its client establish an exact synchronization
barrier before accepting commands.

## Highlights

- Let a new TCP connection replace a stale peer and retain transparent bridge
  ownership until hardware reset.
- Discard orphaned serial bytes while the transparent bridge is disconnected,
  preventing protocol output from being interpreted as firmware commands.
- Synchronize each W98AGENT V9 session with a unique `SYNC` token so delayed
  responses from an abandoned operation cannot satisfy a new command.
- Report a 60-second command timeout without force-terminating the DOS process,
  avoiding the observed Windows 98 WinOldAp lockup.
- Validate local GET destinations before beginning a transfer.

## Hardware acceptance

The included firmware was uploaded to and read back from the reference adapter;
the 289,312-byte application image matched SHA-256 exactly. The exact packaged
W98AGENT V9 binary then passed on a Compaq Presario 5875 at 19200-8-N-1:

- ordinary command execution;
- CRC-protected upload and download;
- deliberate command timeout without killing the DOS child;
- natural completion of that child; and
- synchronized reconnect followed by another command and file download.

The firmware also rebuilt byte-for-byte identically from the tagged source, and
the packaged W98SER/2 test recovered from a deliberately corrupted frame and a
deliberately lost acknowledgement.

## Safety

W98AGENT is unauthenticated and can run commands and access files. Use it only
on a trusted isolated network or through a separately secured VPN. Never expose
TCP port 23 to the public Internet. End every session with `BYE`.
