# Tianmengxing MSPM0G3507 Flashing

This board's Type-C connector exposes a CH340 USB-to-UART interface for the
MSPM0 UART ROM BSL bootloader. It is not an XDS110/J-Link/SWD debug probe.

## Build

In VS Code, run:

```text
Terminal -> Run Build Task... -> MSPM0: Build
```

The build produces:

```text
Debug/LED1.out
Debug/LED1.hex
```

Use `Debug/LED1.hex` for Type-C BSL flashing.

## Flash

Run:

```text
Terminal -> Run Task... -> MSPM0: Flash
```

This opens the LCKFB MSPM0 BSL web flasher:

```text
https://wiki.lckfb.com/storage/html/mspm0-web-flasher/index.html
```

Use `Debug/LED1.hex`.

The required UART BSL baud-rate flow is:

```text
9600 bps initial ROM BSL connection
-> BSL Change Baud Rate command
-> 115200 bps erase / program / verify
```

Do not try to connect to ROM BSL directly at `115200`. MSPM0 ROM BSL starts at
`9600 bps, 8N1`; `115200 bps` is the target baud rate after the BSL baud-rate
change command.

Steps:

1. Connect the Tianmengxing board Type-C port to the computer.
2. Enter BSL mode: hold `BSL`, press `RST`, release `RST`, release `BSL`.
3. Start flashing within 10 seconds.
4. Click the serial-connect button and select the CH340 COM port.
5. Confirm the initial BSL connection is the ROM default `9600 bps, 8N1`.
6. Select `Debug/LED1.hex`.
7. Set the BSL target baud rate / Change Baud Rate option to `115200`.
8. Click the one-click flash/program button.
9. After flashing, reset the board and monitor the normal UART debug output at
   the firmware's existing debug baud rate. Do not change the firmware debug
   UART baud rate just because BSL flashing uses `115200`.

## Fallback

If flashing at `115200` is unstable or fails:

1. Keep using the same `Debug/LED1.hex`.
2. Re-enter BSL mode.
3. Set the BSL baud-rate option back to `9600`.
4. Flash again.
5. Check the CH340 driver, USB cable, hub, and Web Serial browser support.

Do not use `1 Mbps`, `2 Mbps`, or `3 Mbps` for this project until `115200` has
been proven stable.

## Debugging

Type-C BSL supports firmware download only. It does not support source-level
breakpoints, single-step, watch variables, or SWD debug.

For real debugging, connect an external SWD debugger such as XDS110, J-Link, or
CMSIS-DAP to the board's SWD pins, then use a matching VS Code debug
configuration.
