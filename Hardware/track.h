#ifndef TRACK_H_
#define TRACK_H_

#include <stdint.h>

#define TRACK_SENSOR_COUNT (8U)

/*
 * 8-channel infrared tracker over software I2C:
 * SCL = PA26
 * SDA = PA24
 *
 * Raw register bit order from OLED left to right:
 * raw bit7 = x8, bit6 = x1, bit5 = x2, bit4 = x3,
 * raw bit3 = x4, bit2 = x5, bit1 = x6, bit0 = x7.
 * x8 is currently fixed at 1 on this module, so it is displayed but
 * not used for task1 control by default.
 *
 * Logical black bit order used by task code:
 * bit0 = x1, bit1 = x2, ... bit7 = x8.
 *
 * Restore note for the old 5-channel GPIO tracker:
 * OUT1 PA26, OUT2 PA24, OUT3 PB24, OUT4 PA22, OUT5 PB18.
 * The old GPIO mapping is also kept as comments in empty.syscfg.
 */
#ifndef TRACK_BLACK_IS_1
#define TRACK_BLACK_IS_1 (0U) // 黑线电平极性；参考模块黑线为 0，若反了改为 1
#endif

#ifndef TRACK_VALID_SENSOR_MASK
#define TRACK_VALID_SENSOR_MASK (0x7FU) // 参与循迹控制的有效通道；当前默认 x1~x7，x8 固定为 1 只显示不参与控制
#endif

#ifndef TRACK_I2C_ADDR
#define TRACK_I2C_ADDR (0x12U) // 8 路循迹模块 7-bit I2C 地址
#endif

#ifndef TRACK_I2C_ADJUST_REG
#define TRACK_I2C_ADJUST_REG (0x01U) // 模块校准/模式寄存器，沿用参考代码
#endif

#ifndef TRACK_I2C_DATA_REG
#define TRACK_I2C_DATA_REG (0x30U) // 8 路红外二进制读数寄存器
#endif

#ifndef TRACK_SOFT_I2C_DELAY_LOOPS
#define TRACK_SOFT_I2C_DELAY_LOOPS (80U) // 软件 I2C 半周期延时循环数，通信不稳可适当加大
#endif

#ifndef TRACK_DEBUG_LOG_MS
#define TRACK_DEBUG_LOG_MS (500U) // TRACK8 UART 调试日志周期，设 0 可关闭
#endif

typedef uint8_t u8;

void Track_Init(void);
void Track_DiagnosticUpdate(uint32_t now_ms);
void Track_SetUartBaud(uint32_t baud);
uint8_t Track_ReadBits(void);
void Track_ReadArray(uint8_t out[TRACK_SENSOR_COUNT]);
int16_t Track_GetLineError(void);
uint8_t Track_IsLineLost(void);

uint8_t Track_GetRawBits(void);
uint8_t Track_GetBlackBits(void);
int16_t Track_GetLastError(void);
uint32_t Track_GetReadCount(void);
uint8_t Track_GetLastI2cStatus(void);
uint32_t Track_GetI2cErrorCount(void);

/* Compatibility names copied from the STM32 software-I2C reference. */
void IR_I2C_GPIO_Init(void);
void set_adjust_mode(u8 mode);
void deal_IRdata(u8 *x1, u8 *x2, u8 *x3, u8 *x4,
    u8 *x5, u8 *x6, u8 *x7, u8 *x8);

/* Compatibility stubs for the old UART8 bring-up mode. */
uint8_t Track_GetLastRxByte(void);
uint32_t Track_GetRxCount(void);
uint32_t Track_GetFrameCount(void);
uint32_t Track_GetCurrentBaud(void);
uint8_t Track_IsBaudScanLocked(void);

#endif /* TRACK_H_ */
