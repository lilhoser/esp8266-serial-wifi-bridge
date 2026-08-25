# Troubleshooting

## No banner

- Start the terminal before pressing reset.
- Confirm 300-8-N-1.
- Disable DTR, RTS, RTS/CTS, DSR/DTR, and XON/XOFF.
- Confirm the correct COM port and that no other application owns it.
- Expect one brief unreadable ESP8266 ROM message at 74880 baud before the
  firmware switches to 300 baud.

## Repeated garbage

Do not assume the configured baud is the only cause. Verify the exact hardware,
UART route, ground, MAX3232 supply/capacitors, cable type, and control-line
state. Prove modern-host USB serial and external RS-232 paths separately.

## Doubled characters

Disable terminal local echo first. Some carrier/terminal combinations reflect
transmitted bytes in addition to firmware or remote echo. Test with a single
`AT` and confirm that the parser returns one `OK` before changing firmware.

## Wi-Fi output interrupts a command

The v0.1.0 firmware resolves initial association before printing
`SERIALWIFI>`. Confirm the banner identifies build `SERIAL-WIFI-BRIDGE-2`.
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
