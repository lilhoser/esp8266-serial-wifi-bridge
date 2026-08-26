# ESP8266 Serial Wi-Fi Bridge v0.3.0-rc1

This prerelease adds persistent selectable serial rates while preserving the
echo-off command path validated in v0.2.0-rc1.

## Highlights

- Select 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, or 115200 baud.
- Use `BAUD N` or `AT$SB=N`; reset applies the saved value.
- Hold Flash for five seconds while the application is running to recover to
  300 baud without erasing Wi-Fi credentials.
- Query active and saved rates with `BAUD`, `AT$SB?`, or `STATUS`.
- Use `--baud` with the modern-host configuration and whole-line test tools.
- Includes a compatibility guide and project background for The Old Net V4
  adapter, with links to the vendor website, store, and original firmware.

## Hardware acceptance

- Exact application read-back matched the release image.
- USB command path passed 20/20 rounds at both 300 and 115200.
- Saved-rate migration, delayed-until-reset activation, and physical
  Flash-button recovery to 300 all passed without losing Wi-Fi settings.
- The reference The Old Net V4 carrier and Compaq Presario 5875 passed 20/20
  exact external RS-232 rounds and bidirectional TCP/serial payloads at 19200.
- That specific DB9 combination produced UART framing errors at 38400, 57600,
  and 115200. This is a measured compatibility boundary, not a universal
  limit; validate each host/carrier pair.

## Image

- Target: ESP8266, 4 MB flash, 26 MHz crystal, DOUT, 40 MHz flash clock
- Write address: `0x000000`
- Size: 289,360 bytes
- SHA-256: `830C37078DBA622ADF59643A7F1933E93DC712926644042BBA34974AB50C0353`

The optional Windows 98 tools ZIP contains the exact W98TERM V7 and LINEPASS
binaries accepted on the reference Presario. ZIP SHA-256:
`EDDC24EF76B4556474C4AFA2BFF45592105EA0FA314BC05DB429DD5020DA0990`.

Back up the complete existing flash before replacing firmware if restoration
matters. This project is independent of and not endorsed by The Old Net.
