# ESP8266 Serial Wi-Fi Bridge

A deliberately small serial-to-Wi-Fi bridge for vintage computers. It connects
an ESP8266/MAX3232 RS-232 adapter to a local Wi-Fi network and exposes one raw
TCP listener on port 23. Configuration happens locally over the serial port;
there is no cloud service, web interface, telemetry, OTA updater, or embedded
network credential.

The supplied build targets a 4 MB ESP8266 device with a 26 MHz crystal, DOUT
flash mode, normal UART0 on GPIO1/GPIO3, and a selectable serial rate from 300
through 115200 baud with 8 data bits, no parity, and one stop bit.

## The Old Net V4 adapter

This project was created for owners of **The Old Net RS232 Serial WiFi Modem
V4**, an ESP8266/MAX3232 adapter sold through [TheOldNet.com](https://theoldnet.com/),
the [The Old Net store](https://theoldnet.com/store), and
[The Old Net on Tindie](https://www.tindie.com/stores/theoldnet/). The hardware
is useful and readily serviceable, but the supplied/available modem firmware
did not provide reliable DB9 operation in the Windows 98 machines that led to
this project. For the tested legacy unit, no active vendor-support resolution
was available to us, so the hardware needed a maintainable replacement stack.

The [official firmware-binaries repository](https://github.com/TheOldNet/theoldnet-wifi-firmware-binaries)
currently warns that its checked-in images are outdated and directs users to a
separate download package. This repository provides a small, fully documented,
open alternative for the specific use case of a raw serial-to-TCP bridge. It is
independent of and not endorsed by The Old Net. It does not implement the
official Hayes-modem, PPP, or SLIP feature set. See
[Using this firmware on The Old Net V4 hardware](docs/THE-OLD-NET-V4.md) before
flashing.

## Download and flash

Most users do not need to build anything. Download these two files from the
latest GitHub release:

- `esp8266-serial-wifi-bridge-v0.4.0-rc2.bin`
- `esp8266-serial-wifi-bridge-windows98-tools-v0.4.0-rc2.zip` (optional)
- `SHA256SUMS.txt`

On a modern Windows computer, clone or download this repository, open
PowerShell in it, and run:

```powershell
.\scripts\flash.ps1 -Port COM3
```

The script installs a pinned Arduino CLI under `.tools`, verifies the release
image's SHA-256, displays the exact image and port, asks before writing, and
requests post-write verification. It writes the application at `0x000000` and
does not request a full-chip erase.

Read [Flashing](docs/FLASHING.md) before proceeding. Flashing third-party
hardware always carries a recovery risk; preserve the original full flash
first if the existing firmware matters.

## Configure and use

Connect at the saved baud rate—**300-8-N-1** for a new or migrated installation—with
DTR, RTS, hardware flow control, and software flow control disabled. Reset the
bridge and wait. Startup always finishes its Wi-Fi attempt before displaying
the prompt:

```text
VINTAGE SERIAL WIFI BRIDGE 15 TRANSPARENT RC
CONNECTING TO example-network
WIFI CONNECTED - IP <assigned-address>
TCP 23 READY
SERIALWIFI>
```

Type `WIFI` for guided setup, `STATUS` for current state, or `HELP` for all
commands. The firmware does not echo commands or passwords; enable terminal
local echo only if your adapter/cable does not already reflect transmitted
characters. See [Using the bridge](docs/USING.md).

For guided setup from a modern Windows computer, install the Python tools and
run:

```powershell
.\scripts\install-python-tools.ps1
.\.tools\python-venv\Scripts\python.exe .\configure.py --port COM6 --ssid YOUR_SSID
```

The password is collected in a masked window and is not stored locally. Start
the command, then press Reset if the startup banner does not appear
automatically.

For a keyboard-independent round-trip check, run
`.\.tools\python-venv\Scripts\python.exe .\scripts\link-test.py COM6 --baud 300`,
then press Reset. The test submits complete `AT` lines and requires an exact
`OK` response for 20 rounds without changing Wi-Fi configuration or flash
contents. Firmware command echo is intentionally off; this prevents a
carrier's electrical reflection from doubling/interleaving characters with a
second firmware-generated copy.

The firmware has passed guided configuration, persistent baud-change
and physical 300-baud recovery tests, 20-round USB command tests at 300 and
115200, and bidirectional TCP/serial payload acceptance. The reference The Old
Net V4 carrier and Compaq Presario 5875 passed 20/20 external RS-232 rounds at
19200; 38400 and higher produced framing errors on that specific DB9 path.

## Build from source

Windows:

```powershell
.\scripts\build.ps1
```

Linux or macOS with Arduino CLI installed:

```sh
./scripts/build.sh
```

Both workflows pin ESP8266 Arduino core 3.1.2 and use the same board options as
the release. See [Building](docs/BUILDING.md) for prerequisites and manual
commands.

## Repository contents

- `firmware/esp8266-serial-wifi-bridge/` - Arduino firmware source
- `release/` - ready-to-flash image and checksum
- `scripts/` - toolchain, build, flash, and verification helpers
- `configure.py` - optional masked modern-host Wi-Fi configurator
- `test_bridge.py` - optional TCP/serial acceptance client
- `tools/windows98/` - source for the tested terminal, link test, and remote agent
- `tools/w98agent-client.py` - modern W98AGENT serial/TCP controller
- `docs/` - hardware, build, flashing, operation, and troubleshooting guides

Windows 98 users can download the ready-built tools from the GitHub release.
See [Windows 98 terminal and link test](docs/WINDOWS98.md).
For remote commands and reliable file transfer, see
[Remote Windows 98 agent](docs/REMOTE-AGENT.md).

## Safety and scope

This firmware supports standard serial rates from 300 through 115200 baud and
uses TCP port 23. TCP port 23 is unencrypted. Use it only on a trusted,
isolated local network or through a separately secured tunnel. Do not expose it
directly to the public Internet.

The initial release supports only the documented ESP8266/MAX3232 UART route.
Do not flash an image merely because an enclosure or connector looks similar.
Confirm the processor, flash size, crystal, voltage, and UART routing first.

## License

MIT. See [LICENSE](LICENSE).
