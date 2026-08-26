# Troubleshooting

## No banner

- Start the terminal before pressing reset.
- Confirm the saved baud and 8-N-1. New and migrated installs start at 300.
- Disable DTR, RTS, RTS/CTS, DSR/DTR, and XON/XOFF.
- Confirm the correct COM port and that no other application owns it.
- Expect one brief unreadable ESP8266 ROM message at 74880 baud before the
  firmware switches to the saved baud.

If the saved rate is unknown, boot normally, wait at least five seconds, then
hold Flash for five seconds while the application is running. Release after the
Wi-Fi LED blinks. Do not hold Flash during power-on or Reset.

## Repeated garbage

Do not assume the configured baud is the only cause. Verify the exact hardware,
UART route, ground, MAX3232 supply/capacitors, cable type, and control-line
state. Prove modern-host USB serial and external RS-232 paths separately.

## Doubled characters

Current firmware does not echo command characters. Some carrier/terminal
combinations electrically reflect transmitted bytes, which is sufficient to
make typing visible. Disable terminal local echo if characters appear twice.
Run `python scripts/link-test.py PORT` from a modern host to verify complete
`AT`/`OK` transactions without depending on interactive typing.

## Wi-Fi output interrupts a command

The current firmware resolves initial association before printing
`SERIALWIFI>`. Confirm `STATUS` identifies build
`SERIAL-WIFI-BRIDGE-13-VARIABLE-BAUD-RC`.
Older private prototypes printed a prompt too early.

## Wi-Fi fails

- Run `STATUS` and confirm a configuration is saved.
- Re-run `WIFI`, carefully entering the SSID and password.
- Use a compatible 2.4 GHz network; ESP8266 does not support 5 GHz Wi-Fi.
- Keep the device on a trusted isolated network.

## Cursor moves by itself after serial data

Some operating systems may mistake unsolicited serial data for a serial mouse.
Disconnect the adapter, reboot, and inspect the operating system's pointing
device configuration before changing firmware or COM resources.

## Enter sends data but the cursor stays on the same line

Raw TCP peers do not necessarily echo a newline. Use W98TERM V7 or newer on
Windows 98; it renders one local CR/LF after Enter while `REMOTE CONNECTED`.
This display-only newline is not added to the TCP payload.

## Prompt does not return after the TCP client closes

The prompt is disabled during transparent bridge mode. Some disconnects are
reported only after the next serial activity; press Enter once. Expect
`REMOTE DISCONNECTED` followed by `SERIALWIFI>`. Press Reset if the dead
session still does not clear. Use a fresh DOS Prompt after any terminal crash
or abnormal exit so stale screen and console state are not mistaken for live
adapter output.
