# Changelog

## v0.3.0-rc1 - 2026-08-26

- Send a visible PASS acknowledgement to the serial terminal after the
  interactive bidirectional test receives its expected response.
- Add persistent standard baud rates from 300 through 115200, familiar
  `AT$SB` aliases, status reporting, and delayed-until-reset activation.
- Add a five-second Flash-button recovery that restores 300 baud without
  changing Wi-Fi credentials.
- Add W98TERM V7 with rate selection, reflection-safe local display, hidden
  password entry, and a visible newline after Enter in TCP bridge mode; add
  the matching 20-round LINEPASS utility and public Open Watcom build script.
- Document The Old Net V4 hardware, vendor resources, project background,
  compatibility boundaries, and serial-versus-Wi-Fi throughput.
- Pass persistent 300-to-115200 change and physical 300-baud recovery over
  USB. Validate 19200 as the reliable ceiling for the reference V4/Presario
  DB9 pair with 20/20 exact rounds and bidirectional TCP/serial payloads.

## v0.2.0-rc1 - 2026-08-26

- Display and confirm the exact received SSID before password entry.
- Require two identical hidden password entries before saving configuration.
- Reject configuration corrupted by an unreliable serial input path.
- Explicitly assert the carrier's DB9 CTS output and configure RTS as input.
- Add a 100 ms reply-turnaround guard for vintage UART compatibility.
- Disable firmware command and backspace echo so carrier-reflected input cannot
  interleave with a second copy of each character.
- Replace the per-character echo diagnostic with whole-line `AT`/`OK` testing.
- Pass guided Wi-Fi configuration, 20/20 USB command rounds, 20/20 external
  RS-232 command rounds with zero UART errors, and bidirectional TCP/serial
  payload acceptance on the reference hardware.

## v0.1.0

- Initial source and ready-to-flash ESP8266 application image.
- Fixed 300-8-N-1 UART0 bridge with one TCP client on port 23.
- Guided hidden-password Wi-Fi configuration and checksummed EEPROM storage.
- Startup waits for Wi-Fi success or failure before displaying `SERIALWIFI>`.
- Brand-neutral prompt, hostname, source, tooling, and documentation.
- Reproducible Arduino CLI build and guarded flashing workflows.
