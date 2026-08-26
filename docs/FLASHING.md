# Flashing safely

Flashing replaces the application firmware. A wrong image or wrong board
selection can make the serial connector unusable until the correct firmware is
restored over USB.

## Recommended Windows workflow

1. Disconnect the adapter from the vintage computer.
2. Connect only its USB data/power cable to the modern computer.
3. Find its COM port in Device Manager.
4. Preserve the existing full flash if it is not already backed up:

```powershell
.\scripts\backup-flash.ps1 -Port COM3
```

The backup script reads exactly 4 MB, refuses to overwrite an existing file,
and reports its SHA-256. Backups may contain credentials or vendor firmware;
keep them private and never attach them to a public issue or release.

5. Inspect the release image if desired:

```powershell
.\scripts\inspect-image.ps1
```

6. Flash the verified release:

```powershell
.\scripts\flash.ps1 -Port COM3
```

The flash script:

- selects the repository's versioned release image unless `-Binary` is given;
- verifies SHA-256 against `release/SHA256SUMS.txt`;
- prints the exact absolute path, byte length, hash, port, and board profile;
- requires an explicit confirmation unless `-Yes` is supplied; and
- uses Arduino CLI upload verification with sketch-only erase settings.

It does not request `erase_flash` or a full-chip wipe.

NodeMCU-style boards normally enter the uploader automatically. If connection
fails, hold Flash, tap Reset, release Flash, and retry the flash command. This
button sequence is only for USB flashing; do not hold Flash during an ordinary
application Reset.

## Linux or macOS workflow

```sh
./scripts/flash.sh /dev/ttyUSB0
```

## After flashing

Open a terminal at 300-8-N-1 with all flow control off, start the terminal
first, then reset the bridge. A valid startup ends in `SERIALWIFI>` only after
Wi-Fi has succeeded or failed.

Firmware v0.2.0-rc1 settings migrate automatically and begin with saved baud
300. Other vendor or private firmware layouts are not imported; run `WIFI`
once. After configuration, validate faster rates from 300 upward rather than
assuming the maximum advertised value is reliable on a particular DB9 path.
