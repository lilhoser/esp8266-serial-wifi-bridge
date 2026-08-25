# ESP8266 Serial Wi-Fi Bridge v0.1.0

Initial public pre-release for hardware acceptance.

The downloadable `.bin` targets a 4 MB ESP8266 with a 26 MHz crystal, DOUT
flash mode at 40 MHz, normal UART0 GPIO1/GPIO3, and 300-8-N-1 serial operation.
It is an application image for address `0x000000`, not a full-flash image.

Verify `SHA256SUMS.txt` before flashing. Preserve the existing full flash first
if restoration matters. Do not flash this image to hardware with a different
processor, flash size, crystal, or UART route.

This release is marked pre-release until the exact published binary completes
on-hardware startup, configuration, and bidirectional TCP/serial acceptance.
