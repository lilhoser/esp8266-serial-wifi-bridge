# Using this firmware on The Old Net V4 hardware

## Background and relationship

The Old Net RS232 Serial WiFi Modem V4 combines a NodeMCU-style ESP8266 module,
a MAX3232-family RS-232 transceiver, USB serial, a DE9 connector, Flash and
Reset buttons, and status LEDs. It has been sold through:

- [TheOldNet.com](https://theoldnet.com/)
- [The Old Net store](https://theoldnet.com/store)
- [The Old Net Tindie store](https://www.tindie.com/stores/theoldnet/)
- [V4 product listing on Tindie](https://www.tindie.com/products/theoldnet/rs232-serial-wifi-modem-for-vintage-computers-v4/)

The official firmware is a broader Hayes-modem/PPP solution and the official
downloads also include SLIP networking. Start with the vendor material if
those are the features you need:

- [Official firmware-binaries repository](https://github.com/TheOldNet/theoldnet-wifi-firmware-binaries)
- [Upstream modem firmware and guide](https://github.com/ssshake/vintage-computer-wifi-modem)

This independent project arose after a V4 unit could be configured over USB
but its available modem firmware did not produce a reliable interactive DB9
link on either of two vintage PCs. For that tested legacy unit, no active
vendor-support resolution was available to us. Investigation established that
the V4 carrier reflects
host-transmitted bytes back toward the host. A second character echo from
replacement firmware interleaved with that reflected stream and appeared as
random substitutions, duplicates, and control characters. This firmware keeps
command echo off and responds only after a complete line.

The Old Net and its product names belong to their respective owner. This
repository uses those names only to identify compatible hardware. It is not
affiliated with or endorsed by The Old Net.

## What changes when you flash

This image replaces the application firmware at address `0x000000`. It provides
a guided local Wi-Fi setup and one transparent TCP listener on port 23. It does
not emulate a Hayes modem and does not provide PPP, SLIP, AT dialing, Telnet
option negotiation, authentication, or encryption.

Preserve the original complete 4 MB flash before writing if restoration
matters. Confirm all of the following before using the supplied image:

- ESP8266 processor;
- 4 MB flash;
- 26 MHz crystal;
- DOUT flash mode;
- UART0 TX on GPIO1 and RX on GPIO3; and
- a MAX3232-compatible RS-232 stage.

An enclosure marked V4 is useful evidence, but inspect the board rather than
assuming every externally similar adapter has the same route.

## Buttons and LEDs

- **Reset** restarts the ESP8266.
- **Flash** is connected to GPIO0. Do not hold it while powering on or pressing
  Reset; that selects the ESP8266 ROM flashing mode.
- To recover an unknown saved serial rate, boot normally, wait at least five
  seconds, then hold **Flash** for five seconds while the application is
  running. The Wi-Fi LED blinks, the firmware saves 300 baud, and it restarts
  after the button is released.
- The Wi-Fi LED indicates association. The RS232 enclosure LED is not driven by
  this minimal firmware.

## First connection

1. Start a terminal at 300 baud, 8-N-1.
2. Disable DTR, RTS, RTS/CTS, DSR/DTR, and XON/XOFF flow control.
3. Start the terminal before pressing Reset.
4. Wait for the Wi-Fi result and `SERIALWIFI>` prompt.
5. Type `WIFI`, confirm the exact SSID shown by the firmware, enter the hidden
   password twice, and approve the save.
6. Run `STATUS` and record the assigned address.

Some V4 carriers reflect transmitted bytes. Firmware command echo is therefore
disabled. Use a terminal that either displays the reflected copy or provides
one local display while suppressing the reflection. Never add a second echo if
characters are already visible.

## Serial speed

Use `BAUD` or the familiar `AT$SB?` alias to display the active and saved rate.
Save a supported rate with either form:

```text
BAUD 115200
AT$SB=115200
```

Supported values are `300`, `1200`, `2400`, `4800`, `9600`, `19200`, `38400`,
`57600`, and `115200`. The command confirms the save at the current rate. Exit
the terminal, reopen it at the new rate, and then press Reset. The change takes
effect only at boot, which prevents the acknowledgement from disappearing
mid-command.

Use `BAUD RESET` to save 300 baud through the command interface. Use the Flash
button recovery procedure above when the saved rate is unknown.

Higher rates depend on the vintage UART, cable, terminal, and application.
Validate a new rate with `scripts/link-test.py --baud RATE PORT` before using it
for important transfers.

The reference V4 adapter and Compaq Presario 5875 passed 20/20 exact
whole-line tests plus bidirectional TCP/serial payload acceptance at 19200.
The same combined DB9 path produced framing errors at 38400, 57600, and
115200, although the adapter's USB-UART and the Presario serial control path
each passed independently at 115200. The validated ceiling is therefore
19200 for that specific pair; other hosts, cables, and carrier revisions may
differ.
