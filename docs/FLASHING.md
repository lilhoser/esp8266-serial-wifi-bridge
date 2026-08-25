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
5. Run:

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

## Linux or macOS workflow

```sh
./scripts/flash.sh /dev/ttyUSB0
```

## After flashing

Open a terminal at 300-8-N-1 with all flow control off, start the terminal
first, then reset the bridge. A valid startup ends in `SERIALWIFI>` only after
Wi-Fi has succeeded or failed.

The v0.1.0 configuration record format is new. Firmware from an earlier private
prototype will not reuse its saved network configuration; run `WIFI` once.
