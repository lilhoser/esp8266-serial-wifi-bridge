# Release procedure

1. Review source changes and update the version strings.
2. Run `scripts/build.ps1` or `scripts/build.sh` from a clean checkout.
3. Build a second time in a fresh output directory and compare SHA-256 hashes.
4. Inspect the image header: ESP8266, 4 MB, 40 MHz, DOUT, valid checksum.
5. Scan source and binary strings for names, credentials, private addresses,
   workstation paths, and unrelated branding.
6. Flash the exact release candidate to supported hardware.
7. Validate reset ordering, connection success and failure, `AT`, `STATUS`,
   hidden-password setup, clean exit, and bidirectional TCP payload.
8. Copy only the application `.bin` to `release/`; never publish EEPROM or a
   complete flash capture.
9. Update `release/SHA256SUMS.txt` and verify it from disk.
10. Tag the exact commit and attach the `.bin` and checksum file to the release.

Do not call a release accepted solely because compilation or upload reported
success. Hardware read-back and end-to-end serial/network behavior are separate
acceptance gates.
