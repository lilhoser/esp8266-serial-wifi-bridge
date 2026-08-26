# Hardware compatibility

The release image is built for this electrical and firmware profile:

| Item | Required value |
|---|---|
| MCU | ESP8266 |
| Flash | 4 MB |
| Crystal | 26 MHz |
| Flash mode | DOUT |
| Flash frequency | 40 MHz |
| UART | UART0, TX GPIO1, RX GPIO3 |
| RS-232 level conversion | MAX3232-compatible transceiver |
| Serial format | 300 baud, 8-N-1 |
| DB9 control | CTS asserted from GPIO15; RTS sensed at GPIO13 but ignored |
| Flow control | None |
| Wi-Fi indicator | Active-low GPIO16 |

The `.bin` is an application image for address `0x000000`. It is not a full
flash dump and does not contain user Wi-Fi configuration.

## Before flashing

1. Open the enclosure and identify the ESP8266 module and RS-232 transceiver.
2. Confirm that the board uses normal UART0 GPIO1/GPIO3. A board using swapped
   UART pins or a different microcontroller needs a different build.
3. Confirm a 3.3 V ESP8266 supply. RS-232 voltage belongs only on the transceiver
   side; never connect it directly to ESP8266 GPIO.
4. Record the USB serial port and MAC address.
5. Preserve a complete original flash image if restoration may matter.

Do not connect two powered USB hosts or two external power sources unless the
carrier documentation explicitly permits it. Do not hot-plug PS/2 peripherals
on the vintage computer while troubleshooting an unrelated serial problem.
