# ESP8266 Serial Wi-Fi Bridge v0.2.0-rc1

Hardware-tested release candidate for ESP8266/MAX3232 V4-style carriers.

The downloadable application image targets a 4 MB ESP8266 with a 26 MHz
crystal, DOUT flash mode at 40 MHz, normal UART0 on GPIO1/GPIO3, and fixed
300-8-N-1 serial operation. It is written at address `0x000000`; it is not a
full-flash image.

Changes from v0.1.0:

- disables firmware command and backspace echo to prevent a carrier-reflected
  byte stream from interleaving with a second firmware-generated copy;
- waits for a complete CR-terminated line before replying;
- confirms the retained SSID before accepting a password;
- requires two identical hidden password entries before saving;
- completes Wi-Fi association or failure reporting before displaying a prompt;
- explicitly configures the V4 carrier's GPIO13 RTS input and GPIO15 CTS output;
  and
- adds safe whole-line serial acceptance tooling.

The exact 286,800-byte image was compiled twice with byte-identical output,
verified during upload, and independently read back from the target ESP8266.
It passed guided Wi-Fi setup, 20/20 whole-line USB serial rounds, and 20/20
whole-line external RS-232 rounds on a Windows 98 computer with zero reported
UART errors. Final acceptance passed in both network directions: a fixed TCP
payload reached the Windows 98 serial endpoint exactly, and a separate typed
serial response reached the TCP client exactly.

SHA-256:

```text
8692462AB79C3F28958D04A2E879259068BCF4ABF69B894D6A4AB9098949130E  esp8266-serial-wifi-bridge-v0.2.0-rc1.bin
```

The `rc1` label is retained for broader community hardware coverage; the exact
published image completed all acceptance gates on the reference hardware.
