# Building from source

Release builds use:

- Arduino CLI 1.5.1
- ESP8266 Arduino core 3.1.2
- board `esp8266:esp8266:generic`
- 4 MB flash, DOUT, 40 MHz flash, 26 MHz crystal
- sketch-only erase policy

## Windows automated build

From PowerShell in the repository root:

```powershell
.\scripts\build.ps1
```

If Arduino CLI is absent, `bootstrap-arduino-cli.ps1` downloads the official
1.5.1 Windows archive, verifies its pinned SHA-256, installs it under `.tools`,
and installs ESP8266 core 3.1.2. Nothing is installed system-wide.

The application image is written to:

```text
build/esp8266-serial-wifi-bridge.ino.bin
```

## Linux or macOS build

Install Arduino CLI using its official instructions, then run:

```sh
chmod +x scripts/*.sh
./scripts/build.sh
```

## Manual build

```sh
arduino-cli config add board_manager.additional_urls \
  https://arduino.esp8266.com/stable/package_esp8266com_index.json
arduino-cli core update-index
arduino-cli core install esp8266:esp8266@3.1.2
arduino-cli compile \
  --fqbn 'esp8266:esp8266:generic:eesz=4M,FlashMode=dout,FlashFreq=40,CrystalFreq=26,wipe=none' \
  --output-dir build .
```

Run `scripts/verify-release.ps1` or `scripts/verify-release.sh` to compare a
local build with the published checksum. A source change is expected to change
the hash; update the release manifest only as part of a reviewed new release.
