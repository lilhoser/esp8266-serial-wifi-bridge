ESP8266 SERIAL WI-FI BRIDGE - WINDOWS 98 TOOLS

W98TERM.EXE [baud]
  Interactive COM1 terminal. Default: 300 baud, 8-N-1, no flow control.
  Start W98TERM, then press adapter Reset. Press Escape to exit.
  Example: C:\W98TERM 19200

LINEPASS.EXE [baud]
  Runs 20 exact reflected-command/firmware-reply rounds on COM1 and writes
  C:\LINEPASS.TXT. Use only after the correct W98TERM banner is visible.
  Example: C:\LINEPASS 19200

W98AGENT.EXE [baud]
  Remote command and reliable file-transfer agent for COM1. Default: 115200.
  With the Wi-Fi bridge, connect the modern TCP client FIRST, then run:
  C:\W98AGENT 19200
  Use W98SER/2 only on a trusted isolated network. Type BYE in the client to
  stop the agent cleanly.
  V9 requires the client SYNC barrier and never force-kills a timed-out DOS
  child. Do not run PAUSE or other interactive commands through EXEC.

Supported rates:
  300 1200 2400 4800 9600 19200 38400 57600 115200

Notes:
  - A brief garbage line before the banner is the ESP8266 ROM at 74880 baud.
  - Build 15 latches transparent mode after the first TCP connection.
  - Reconnect a client to resume. Press Reset to restore SERIALWIFI>.
  - Hold Flash for five seconds while running to restore saved baud to 300.
  - Do not hold Flash during power-on or Reset; that selects ROM bootloader.

Full documentation:
  https://github.com/lilhoser/esp8266-serial-wifi-bridge
  See docs/REMOTE-AGENT.md for the complete agent workflow.
