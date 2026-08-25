# ESP8266 Serial Wi-Fi Bridge

A deliberately small serial-to-Wi-Fi bridge for vintage computers. It connects
an ESP8266/MAX3232 RS-232 adapter to a local Wi-Fi network and exposes one raw
TCP listener on port 23. Configuration happens locally over the serial port;
there is no cloud service, web interface, telemetry, OTA updater, or embedded
network credential.

The supplied build targets a 4 MB ESP8266 device with a 26 MHz crystal, DOUT
flash mode, normal UART0 on GPIO1/GPIO3, and a fixed serial profile of
300 baud, 8 data bits, no parity, and one stop bit.

## Download and flash

Most users do not need to build anything. Download these two files from the
latest GitHub release:

- `esp8266-serial-wifi-bridge-v0.1.0.bin`
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

Connect at **300-8-N-1** with DTR, RTS, hardware flow control, and software flow
control disabled. Reset the bridge and wait. Startup always finishes its Wi-Fi
attempt before displaying the prompt:

```text
VINTAGE SERIAL WIFI BRIDGE 2
CONNECTING TO example-network
WIFI CONNECTED - IP <assigned-address>
TCP 23 READY
SERIALWIFI>
```

Type `WIFI` for guided setup, `STATUS` for current state, or `HELP` for all
commands. Wi-Fi passwords are not echoed. See [Using the bridge](docs/USING.md).

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

- `esp8266-serial-wifi-bridge.ino` - firmware source
- `release/` - ready-to-flash image and checksum
- `scripts/` - toolchain, build, flash, and verification helpers
- `configure.py` - optional masked modern-host Wi-Fi configurator
- `test_bridge.py` - optional TCP/serial acceptance client
- `docs/` - hardware, build, flashing, operation, and troubleshooting guides

## Safety and scope

This firmware is intentionally fixed at 300 baud and TCP port 23. TCP port 23
is unencrypted. Use it only on a trusted, isolated local network or through a
separately secured tunnel. Do not expose it directly to the public Internet.

The initial release supports only the documented ESP8266/MAX3232 UART route.
Do not flash an image merely because an enclosure or connector looks similar.
Confirm the processor, flash size, crystal, voltage, and UART routing first.

## License

MIT. See [LICENSE](LICENSE).
