# Using the bridge

## Serial settings

- 300 baud
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
| `CONNECT` or `ATC1` | Retry using saved network settings |
| `HANGUP` or `ATH` | Close the active TCP client |
| `HELP`, `AT?`, or `ATHELP` | Command list |

The exact received SSID is displayed and must be confirmed before password
entry. Password input is hidden and must be entered twice identically before
it can be saved. Setup accepts WPA/WPA2-style ASCII passphrases from 8 through
63 characters. The firmware stores the confirmed SSID and password in a
checksummed EEPROM record on the device.

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

Only one TCP client is accepted at a time on port 23. Once connected, bytes are
passed directly between TCP and the serial port. There is no Telnet option
negotiation, encryption, authentication, or character-set conversion.
