# Using the bridge

## Serial settings

- saved baud rate; 300 for a new or migrated installation
- 8 data bits
- no parity
- 1 stop bit
- no RTS/CTS
- no XON/XOFF
- DTR and RTS disabled where the terminal permits it

Start the terminal before resetting the bridge so the one-time banner is not
lost.

Firmware command echo is disabled. Some MAX3232 carrier boards electrically
reflect transmitted characters back to the terminal, so typing remains visible
without any software echo. If your hardware does not, enable terminal local
echo. Never enable local echo when characters are already visible, or each
keystroke will appear twice. Backspace is still interpreted by the command-line
editor even when no erase sequence is transmitted.

## Commands

| Command | Purpose |
|---|---|
| `AT` | Presence check; returns `OK` |
| `WIFI` | Guided SSID/password setup and connection |
| `STATUS` or `ATI` | Configuration, Wi-Fi, IP, TCP, and remote state |
| `BAUD` or `AT$SB?` | Active, saved, and supported serial rates |
| `BAUD N` or `AT$SB=N` | Save a rate; Reset applies it |
| `BAUD RESET` | Save 300 baud |
| `CONNECT` or `ATC1` | Retry using saved network settings |
| `HANGUP` or `ATH` | Close the active TCP client |
| `HELP`, `AT?`, or `ATHELP` | Command list |

The exact received SSID is displayed and must be confirmed before password
entry. Password input is hidden and must be entered twice identically before
it can be saved. Setup accepts WPA/WPA2-style ASCII passphrases from 8 through
63 characters. The firmware stores the confirmed SSID and password in a
checksummed EEPROM record on the device.

## Changing serial speed

Supported rates are 300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, and
115200 baud. `BAUD N` saves the requested value but deliberately leaves the
current session unchanged. After the acknowledgement:

1. exit the terminal;
2. reopen it at the new rate; and
3. press Reset on the adapter.

`STATUS` reports both active and saved values. Wi-Fi credentials survive a baud
change. Firmware v0.2.0-rc1 settings are migrated automatically with 300 baud
as their initial saved rate.

For recovery, boot normally and wait at least five seconds. Then hold the Flash
button for five seconds while the firmware is running. Release it after the
Wi-Fi LED blinks; the adapter restarts at 300 baud. Do not hold Flash during
power-on or Reset because GPIO0 low selects the ESP8266 ROM bootloader.

## Serial speed versus Wi-Fi speed

The `BAUD` setting changes only UART/RS-232 communication between the vintage
computer and adapter. It does not change the ESP8266's 2.4 GHz 802.11b/g/n
Wi-Fi association or TCP connection. The radio negotiates its own link rate
with the access point; it does not run at the selected serial baud. End-to-end
payload throughput is nevertheless limited by the serial side because every
byte entering or leaving the vintage computer must cross that UART.

Ignoring protocol overhead, 8-N-1 carries roughly one payload byte per ten
serial bits:

| Baud | Approximate raw rate | Approximate time per MiB |
|---:|---:|---:|
| 300 | 30 B/s | 9.7 hours |
| 9,600 | 960 B/s | 18 minutes |
| 19,200 | 1,920 B/s | 9.1 minutes |
| 38,400 | 3,840 B/s | 4.6 minutes |
| 57,600 | 5,760 B/s | 3.0 minutes |
| 115,200 | 11,520 B/s | 1.5 minutes |

Real file transfers are slower. Validate the fastest reliable rate for the
specific UART, cable, and software before moving important data.

On the reference The Old Net V4 carrier connected directly to a Compaq
Presario 5875, 19200 passed all 20 whole-line rounds with exact payloads and
also passed a bidirectional TCP/serial token test. Rates of 38400, 57600, and
115200 failed the external DB9 stress test with UART framing errors, even
though the adapter USB-UART and the Presario/null-modem control path each
passed separately at 115200. This is a measured compatibility result for that
combination, not a universal limit. Start at 300 and validate before selecting
a faster saved rate.

## Startup behavior

If configuration exists, startup waits up to 20 seconds for association. It
prints one definitive result and only then displays `SERIALWIFI>`:

```text
CONNECTING TO example-network
WIFI CONNECTED - IP <assigned-address>
TCP 23 READY
SERIALWIFI>
```

or:

```text
CONNECTING TO example-network
WIFI CONNECTION FAILED
SERIALWIFI>
```

This ordering prevents asynchronous connection messages from overwriting a
command being typed at the prompt.

## TCP bridge

Only one TCP client owns the bridge at a time on port 23. A newly accepted TCP
connection replaces the previous peer, including a stale peer that the ESP8266
network stack still reports as connected. Once connected, bytes are passed
directly between TCP and the serial port. There is no Telnet option negotiation,
encryption, authentication, or character-set conversion.

The `SERIALWIFI>` command prompt is intentionally unavailable after a TCP
client claims the bridge because every serial byte is bridge payload. Build 15
latches transparent mode until hardware reset. If the TCP client disappears,
serial bytes are discarded rather than parsed as firmware commands; this keeps
an unattended vintage-host agent from entering a feedback loop with the local
console. Connect a new client to resume bridging, or press hardware Reset to
return to command mode. A new client replaces a stale prior peer immediately.

Windows 98 users should see [Windows 98 terminal and link test](WINDOWS98.md)
for local display, fresh-session, boot-garbage, and rate-validation behavior.

## End-to-end acceptance

From a modern computer that can reach the bridge address, run:

```powershell
python .\test_bridge.py 192.0.2.10 --reply "SERIAL LINK OK"
```

Replace the documentation address with the bridge's reported address. The
tool connects to TCP port 23 and sends instructions to the serial display.
Type the requested reply on the serial terminal and press Enter. A successful
test sends a visible `PASS - BIDIRECTIONAL BRIDGE CONFIRMED` message back to
the vintage terminal before the TCP client closes.
