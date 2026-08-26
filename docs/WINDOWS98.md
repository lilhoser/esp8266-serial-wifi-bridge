# Windows 98 terminal and link test

The release includes two 32-bit Windows console programs for systems where
HyperTerminal is absent or unsuitable. They use Win32 serial APIs available in
Windows 98 and require no installation:

- `W98TERM.EXE` is the interactive terminal.
- `LINEPASS.EXE` is a 20-round exact external-RS-232 stress test for carriers
  that reflect host TX back to host RX, including the tested The Old Net V4.

Copy the files to the Windows 98 computer with a floppy, CD-R, existing network
connection, or a trusted null-modem transfer. They fit together on a 1.44 MB
floppy.

## W98TERM

Close HyperTerminal and every other program using COM1. Open a fresh MS-DOS
Prompt and run the terminal at the adapter's saved rate:

```bat
C:\W98TERM 19200
```

`W98TERM` defaults to 300 when no rate is supplied. It supports the same nine
rates as the firmware, always uses COM1 with 8-N-1, and disables DTR, RTS,
hardware flow control, and software flow control. Start W98TERM first and then
press Reset on the adapter so the complete application banner is visible.

A brief garbage line before the correct banner is normal: the ESP8266 ROM emits
its boot message at 74880 baud before the application switches to the saved
rate.

W98TERM displays ordinary outgoing characters locally once and suppresses the
carrier-reflected copy. It automatically hides local display at firmware
password prompts. While `REMOTE CONNECTED` is active, pressing Enter displays
one local newline so the operator is not left at the end of the submitted
line. Press Escape to exit and restore the previous COM1 configuration.

Use a fresh DOS Prompt after changing rates or after a serial program exits
abnormally. Closing the window removes stale screen text and releases console
state left by older terminal programs.

## Command mode versus bridge mode

`SERIALWIFI>` is available only when no TCP client is connected. During
`REMOTE CONNECTED`, all serial bytes are payload and firmware commands are not
interpreted. Close the TCP client to return to command mode.

On the tested ESP8266 stack, a peer that has just closed may not be reported as
disconnected until the next serial activity. If the prompt does not return,
press Enter once. The firmware then prints `REMOTE DISCONNECTED` followed by
`SERIALWIFI>`. Press Reset if a dead TCP session still does not clear.

## LINEPASS

First use W98TERM to confirm the correct banner at the rate under test. Exit
W98TERM, close that DOS window, open a fresh MS-DOS Prompt, and run:

```bat
C:\LINEPASS 19200
```

LINEPASS sends 20 complete mixed-case command lines. It separately validates
the carrier's exact 65-byte reflected command and the firmware's exact 33-byte
reply, while recording Win32 UART error flags. Results are also written to
`C:\LINEPASS.TXT`.

Do not select a rate merely because a banner is readable. Accept it only after
all 20 rounds pass with no UART errors. The reference V4/Presario pair passed
at 19200 and failed at 38400 and above.

## Building the tools

Install [Open Watcom v2](https://github.com/open-watcom/open-watcom-v2), set
the `WATCOM` environment variable to its installation directory, then run from
PowerShell:

```powershell
.\scripts\build-windows98-tools.ps1
```

The executables are written under `build\windows98`. The GitHub release also
provides ready-built copies, so Windows 98 users do not need a compiler.

The hardware-tested v0.3.0-rc1 package contains:

| File | Size | SHA-256 |
|---|---:|---|
| `W98TERM.EXE` | 33,280 | `CF2E350A3B0ACFF9C5ED5BC0C0BE2EA674A62DDCDFE5F8BEF2B1493142B82987` |
| `LINEPASS.EXE` | 35,328 | `F2F5500828E448F95AC4197183D7E84B3FA6577285A11C5853BC33668B5EF90D` |

The ZIP itself is covered by `release/SHA256SUMS.txt`.
