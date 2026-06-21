# MSPM0 Track I2C Reference Report

Reference PDF:

```text
d:\24电赛h题\个人代码\八路巡线红外传感器\1.单板主控\3.MSPM0G3507\I2C\MSPM0-I2C.pdf
```

## PDF Address Review

The PDF text that can be extracted does not contain an explicit I2C device
address definition such as:

```text
SLAVE_ADDR
I2C_ADDR
SENSOR_ADDR
Track_ADDR
Target address
```

The visible source fragment contains only:

```c
IRI2C_WriteByte(0x01, 1);
IRI2C_WriteByte(0x01, 0);
printf_i2c_data();
```

Context:

```text
0x01 is the first argument of IRI2C_WriteByte().
1 is the data value used to enter calibration.
0 is the data value used to exit calibration.
```

Therefore `0x00` is not an I2C device address. It is only the data value in
`IRI2C_WriteByte(0x01, 0)`.

## Current Address Decision

Because the PDF does not provide a device address, firmware must not invent one.
The current known candidates remain:

```text
0x12 - earlier STM32/source candidate, 7-bit form
0x24 - possible 8-bit/shifted form or possible 7-bit candidate to scan
third scan address - any address found besides OLED 0x3C and MPU6050 0x68
```

If scan does not find one of those candidates, `Track_Init()` returns
`TRACK_STATUS_NOT_FOUND` and does not access address `0x00`.

## Pin Definitions In PDF

The PDF wiring table shows:

```text
MSPM0G3507 PA0 -> track SDA
MSPM0G3507 PA1 -> track SCL
5V -> track 5V
GND -> track GND
```

The PDF title includes `MSPM0-IO`, and the code uses `IRI2C_*` functions. The
PDF does not include the implementation of those functions, so it does not show
whether the reference project used hardware I2C or GPIO bit-banged I2C.

## Visible PDF Init Flow

The visible main function is:

```c
int main(void)
{
    SYSCFG_DL_init();

    delay_ms(1000);
    delay_ms(1000);

    NVIC_ClearPendingIRQ(MYUART_INST_INT_IRQN);
    NVIC_EnableIRQ(MYUART_INST_INT_IRQN);

    printf("start\r\n");

    IRI2C_WriteByte(0x01, 1); // enter calibration
    delay_ms(200);
    IRI2C_WriteByte(0x01, 0); // exit calibration
    delay_ms(200);

    printf("okok\r\n");

    while (1) {
        printf_i2c_data();
        delay_ms(500);
    }
}
```

The PDF also says to hold the MSPM0 reset before power-on, wait until the IR
module works normally, then release reset; otherwise I2C can deadlock.

## Firmware Behavior After Fix

```text
1. Address 0x00 is forbidden and is never used as a track device address.
2. If scan finds only 0x3C/0x68, OLED displays ADDR? and UART prints
   [TRACK] pdf addr unknown.
3. If scan finds 0x12, 0x24, or another third address, Track_Init runs once
   for that visible candidate.
4. If the visible candidate NAKs, track is disabled and no protocol guessing
   loop is started.
5. Direct diagnostics remain disabled by default.
```

## Final Adopted Mode

```text
Address:   unknown from PDF; use only 0x12/0x24/third scan candidate
0x00:      not an address, forbidden
Command:   visible PDF command/register argument 0x01
Data:      1 enters calibration, 0 exits calibration
Delay:     2000 ms before init, then 200 ms after each command
Read mode: PDF_REF only after a valid candidate address is visible
Read reg:  0x30 is retained from earlier source, not proven by the PDF
Pins:      PA0=SDA, PA1=SCL
I2C speed: unchanged, 100 kHz
```

If the PDF reference sequence still cannot get an ACK from a visible candidate,
the next step is hardware-level checking: PA0/PA1 idle voltage, module power,
module mode, common ground, and I2C level shifting.
