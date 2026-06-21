# Track Module Port Report

## Reference Source

Reference files checked:

```text
d:\24... \24H\Hareware\track.c
d:\24... \24H\Hareware\track.h
```

The STM32 reference code uses software I2C:

```c
/* PA7 = SCL, PA6 = SDA */
#define IR_I2C_SCL_PIN GPIO_PIN_7
#define IR_I2C_SDA_PIN GPIO_PIN_6
```

The reference header defines the track module address as a 7-bit I2C address:

```c
#define IR_I2C_ADDR 0x12
```

The reference comments also show the STM32 HAL 8-bit bus address form:

```c
HAL_I2C_Mem_Write(&hi2c2, (0x12<<1), 0x01, ..., &mode, 1, 10);
HAL_I2C_Mem_Read(&hi2c2, (0x12<<1), 0x30, ..., buf, 1, 10);
```

Address mapping:

```text
STM32 reference 7-bit address: 0x12
STM32 HAL/bus address:         0x24 = 0x12 << 1
MSPM0 platform_i2c address:    0x12
```

## Reference Write Flow

The STM32 function `Soft_I2C_WriteReg(dev_addr, reg, data)` does:

```text
START
send dev_addr << 1
wait ACK
send register address
wait ACK
send data byte
wait ACK
STOP
```

It is used by:

```c
set_adjust_mode(mode)
Soft_I2C_WriteReg(IR_I2C_ADDR, 0x01, mode);
```

So the adjust/config register is:

```text
register: 0x01
value 0: exit adjust mode
value 1: enter adjust mode
```

## Reference Read Flow

The STM32 function `Soft_I2C_ReadReg(dev_addr, reg, data)` does:

```text
START
send dev_addr << 1
wait ACK
send register address
wait ACK
REPEATED START
send (dev_addr << 1) | 1
wait ACK
read one byte
send NACK
STOP
```

It is used by:

```c
read_IRdata(buf)
Soft_I2C_ReadReg(IR_I2C_ADDR, 0x30, buf);
```

So the digital state register is:

```text
register: 0x30
length:   1 byte
```

Reference bit mapping:

```text
x1 = bit7
x2 = bit6
x3 = bit5
x4 = bit4
x5 = bit3
x6 = bit2
x7 = bit1
x8 = bit0
```

## MSPM0 Port

The current MSPM0 project uses hardware I2C0:

```text
PA0 = SDA
PA1 = SCL
platform_i2c APIs use 7-bit addresses
```

Final MSPM0 definitions:

```text
TRACK_I2C_ADDR          = 0x12
TRACK_I2C_ADDR_8BIT     = 0x24
TRACK_REG_ADJUST_MODE   = 0x01
TRACK_REG_DIGITAL_STATE = 0x30
```

Final MSPM0 flow:

```text
1. Use i2c_scan result to confirm 0x12 is present.
2. If 0x12 is not present, disable track reads.
3. If 0x12 is present, write reg 0x01 = 0.
4. Read reg 0x30, one byte, with platform_i2c_read_u8().
5. Display the raw byte as BIN and HEX.
```

The old generic raw/cmd/multi-register protocol guessing was removed. The port now follows only the STM32 reference protocol.

## Current Bus Visibility

Current UART scan result reported by the user:

```text
I2C scan found 0x3C
I2C scan found 0x68
TRACK 0x12 not found
```

If scan still only finds 0x3C and 0x68 after this firmware, the track module is not visible on the MSPM0 I2C bus. That is a hardware/bus visibility problem, not an OLED display issue and not a register selection issue.

Check:

```text
1. The track module SDA/SCL are actually connected to MSPM0 PA0/PA1.
2. VCC and GND are valid and share ground with MSPM0.
3. The module supports 3.3V I2C logic.
4. Pull-ups are present and are not pulled directly to 5V into MSPM0 IO.
5. The module is powered in I2C mode, not another output mode.
6. The STM32 reference used PA7/PA6 software I2C, so wiring may differ.
```
