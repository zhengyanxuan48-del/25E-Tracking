# Track Debug Summary

## Current Scan Result

The current MSPM0G3507 hardware I2C0 bus scan only sees:

```text
OLED    0x3C
MPU6050 0x68
```

The track module is not visible on the bus:

```text
0x09 NAK
0x12 NAK
0x24 NAK
```

## Direct Diagnostics Result

Direct diagnostics were tried against both possible track addresses:

```text
addr=0x12 cmd=0x01           NAK
addr=0x12 write30-read1      NAK
addr=0x12 write30-read8      NAK
addr=0x12 reg=0x30           NAK
addr=0x12 raw1/raw8          NAK
addr=0x24 cmd=0x01           NAK
```

This means the module does not ACK during the I2C address phase. The current
failure is not a register/protocol issue.

## Software Conclusion

The STM32 reference protocol is still:

```text
addr7  = 0x12
addr8  = 0x24
config = write reg 0x01
data   = read reg 0x30
```

Because both 0x12 and 0x24 NAK at the address phase, protocol guessing is now
disabled by default:

```c
#define TRACK_ENABLE_DIRECT_DIAG 0
```

Firmware now runs a hardware visibility protection mode:

```text
1. Scan every 2 seconds.
2. If only 0x3C/0x68 are visible, do not call Track_ReadBits.
3. If a third address appears, print it as a candidate.
4. Try Track_Init once for that candidate.
```

## Hardware Focus

Current hardware notes:

```text
Track module VCC = 5V
MSPM0G3507 IO    = 3.3V
PA0/SDA has 1k series resistance
PA1/SCL has 1k series resistance
```

Important: 1k series resistors are not I2C level shifters. If the track module
pulls SDA/SCL up to 5V, MSPM0 IO can be outside its safe logic range.

Measure:

```text
PA0/SDA idle voltage
PA1/SCL idle voltage
```

If either line idles above about 3.6V, add a real bidirectional I2C level
shifter, such as a BSS138-based module or PCA9306.

## Isolation Tests

Recommended next tests:

```text
1. Connect only the track module to PA0/PA1, VCC, and GND. Scan again.
2. Connect OLED + track only. Scan again.
3. Disconnect MPU6050 and scan OLED + track.
4. If track alone is still invisible, check module power, I2C mode, address,
   and board damage.
5. If track alone works but shared bus fails, check pullups, voltage level,
   bus load, and address conflicts.
```
