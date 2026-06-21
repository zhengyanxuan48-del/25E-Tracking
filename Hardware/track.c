#include "track.h"

#include "debug_uart.h"
#include "ti_msp_dl_config.h"

#include <stddef.h>

/*
 * 5-channel GPIO backup for restore:
 * - bit0 = OUT1 = PA26
 * - bit1 = OUT2 = PA24
 * - bit2 = OUT3 = PB24
 * - bit3 = OUT4 = PA22
 * - bit4 = OUT5 = PB18
 *
 * Previous implementation read those five GPIO inputs directly and used
 * weights {-4, -2, 0, 2, 4}. SysConfig still contains the old TRACK_OUT1..5
 * GPIO entries as a restoration reference, but this file now reconfigures
 * PA26/PA24 at runtime for 8-channel infrared module software I2C.
 *
 * Old 5-channel restore skeleton:
 *   TRACK_SENSOR_COUNT = 5
 *   TRACK_BLACK_IS_1   = 1
 *   OUT1 PA26, OUT2 PA24, OUT3 PB24, OUT4 PA22, OUT5 PB18
 *   weights = {-4, -2, 0, 2, 4}
 *   Track_ReadBits() = read the five GPIO pins and pack OUT1..OUT5 into
 *   logical bits bit0..bit4.
 */

#define TRACK_I2C_SCL_PORT (GPIOA)
#define TRACK_I2C_SCL_PIN  (DL_GPIO_PIN_24)
#define TRACK_I2C_SCL_IOMUX (IOMUX_PINCM59)

#define TRACK_I2C_SDA_PORT (GPIOA)
#define TRACK_I2C_SDA_PIN  (DL_GPIO_PIN_26)
#define TRACK_I2C_SDA_IOMUX (IOMUX_PINCM54)

static const int8_t g_track_weight[TRACK_SENSOR_COUNT] = {
    -7, -5, -3, -1, 1, 3, 5, 7
};

/*
 * Logical sensor index: 0=x1, 1=x2, ... 7=x8.
 * Raw byte order from bit7 to bit0: x8,x1,x2,x3,x4,x5,x6,x7.
 */
static const uint8_t g_track_raw_bit_for_sensor[TRACK_SENSOR_COUNT] = {
    6U, 5U, 4U, 3U, 2U, 1U, 0U, 7U
};

static uint8_t g_track_raw_bits;
static uint8_t g_track_black_bits;
static uint8_t g_track_lost = 1U;
static uint8_t g_track_last_i2c_status;
static int16_t g_track_last_error;
static uint32_t g_track_read_count;
static uint32_t g_track_i2c_error_count;
static uint32_t g_track_last_diag_ms;

static void Track_I2C_Delay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < TRACK_SOFT_I2C_DELAY_LOOPS; i++) {
    }
}

static void Track_I2C_PinInputPullup(uint32_t iomux)
{
    DL_GPIO_initDigitalInputFeatures(iomux,
        DL_GPIO_INVERSION_DISABLE,
        DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE,
        DL_GPIO_WAKEUP_DISABLE);
}

static void Track_I2C_Release(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_disableOutput(port, pin);
}

static void Track_I2C_DriveLow(GPIO_Regs *port, uint32_t pin)
{
    DL_GPIO_clearPins(port, pin);
    DL_GPIO_enableOutput(port, pin);
}

static void Track_I2C_SclHigh(void)
{
    Track_I2C_Release(TRACK_I2C_SCL_PORT, TRACK_I2C_SCL_PIN);
}

static void Track_I2C_SclLow(void)
{
    Track_I2C_DriveLow(TRACK_I2C_SCL_PORT, TRACK_I2C_SCL_PIN);
}

static void Track_I2C_SdaHigh(void)
{
    Track_I2C_Release(TRACK_I2C_SDA_PORT, TRACK_I2C_SDA_PIN);
}

static void Track_I2C_SdaLow(void)
{
    Track_I2C_DriveLow(TRACK_I2C_SDA_PORT, TRACK_I2C_SDA_PIN);
}

static uint8_t Track_I2C_ReadSda(void)
{
    return (DL_GPIO_readPins(TRACK_I2C_SDA_PORT, TRACK_I2C_SDA_PIN) != 0U) ?
        1U : 0U;
}

void IR_I2C_GPIO_Init(void)
{
    Track_I2C_PinInputPullup(TRACK_I2C_SCL_IOMUX);
    Track_I2C_PinInputPullup(TRACK_I2C_SDA_IOMUX);

    DL_GPIO_clearPins(TRACK_I2C_SCL_PORT, TRACK_I2C_SCL_PIN);
    DL_GPIO_clearPins(TRACK_I2C_SDA_PORT, TRACK_I2C_SDA_PIN);
    Track_I2C_SclHigh();
    Track_I2C_SdaHigh();
}

static void Track_I2C_Start(void)
{
    Track_I2C_SdaHigh();
    Track_I2C_SclHigh();
    Track_I2C_Delay();

    Track_I2C_SdaLow();
    Track_I2C_Delay();

    Track_I2C_SclLow();
    Track_I2C_Delay();
}

static void Track_I2C_Stop(void)
{
    Track_I2C_SclLow();
    Track_I2C_SdaLow();
    Track_I2C_Delay();

    Track_I2C_SclHigh();
    Track_I2C_Delay();

    Track_I2C_SdaHigh();
    Track_I2C_Delay();
}

static uint8_t Track_I2C_WaitAck(void)
{
    uint8_t nack;

    Track_I2C_SdaHigh();
    Track_I2C_Delay();

    Track_I2C_SclHigh();
    Track_I2C_Delay();

    nack = (Track_I2C_ReadSda() != 0U) ? 1U : 0U;

    Track_I2C_SclLow();
    Track_I2C_Delay();

    return nack;
}

static void Track_I2C_SendAck(uint8_t ack)
{
    Track_I2C_SclLow();

    if (ack != 0U) {
        Track_I2C_SdaLow();
    } else {
        Track_I2C_SdaHigh();
    }

    Track_I2C_Delay();
    Track_I2C_SclHigh();
    Track_I2C_Delay();
    Track_I2C_SclLow();
    Track_I2C_Delay();
    Track_I2C_SdaHigh();
}

static uint8_t Track_I2C_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0U; i < 8U; i++) {
        Track_I2C_SclLow();

        if ((data & 0x80U) != 0U) {
            Track_I2C_SdaHigh();
        } else {
            Track_I2C_SdaLow();
        }

        data <<= 1U;
        Track_I2C_Delay();
        Track_I2C_SclHigh();
        Track_I2C_Delay();
        Track_I2C_SclLow();
        Track_I2C_Delay();
    }

    return Track_I2C_WaitAck();
}

static uint8_t Track_I2C_ReadByte(uint8_t send_ack)
{
    uint8_t i;
    uint8_t data = 0U;

    Track_I2C_SdaHigh();

    for (i = 0U; i < 8U; i++) {
        Track_I2C_SclLow();
        Track_I2C_Delay();
        Track_I2C_SclHigh();
        Track_I2C_Delay();

        data <<= 1U;
        if (Track_I2C_ReadSda() != 0U) {
            data |= 0x01U;
        }

        Track_I2C_Delay();
    }

    Track_I2C_SclLow();
    Track_I2C_SendAck(send_ack);

    return data;
}

static uint8_t Track_I2C_WriteReg(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    Track_I2C_Start();

    if (Track_I2C_WriteByte((uint8_t)(dev_addr << 1U)) != 0U) {
        Track_I2C_Stop();
        return 1U;
    }

    if (Track_I2C_WriteByte(reg) != 0U) {
        Track_I2C_Stop();
        return 2U;
    }

    if (Track_I2C_WriteByte(data) != 0U) {
        Track_I2C_Stop();
        return 3U;
    }

    Track_I2C_Stop();
    return 0U;
}

static uint8_t Track_I2C_ReadReg(uint8_t dev_addr, uint8_t reg, uint8_t *data)
{
    if (data == NULL) {
        return 0xFFU;
    }

    Track_I2C_Start();

    if (Track_I2C_WriteByte((uint8_t)(dev_addr << 1U)) != 0U) {
        Track_I2C_Stop();
        return 1U;
    }

    if (Track_I2C_WriteByte(reg) != 0U) {
        Track_I2C_Stop();
        return 2U;
    }

    Track_I2C_Start();

    if (Track_I2C_WriteByte((uint8_t)((dev_addr << 1U) | 0x01U)) != 0U) {
        Track_I2C_Stop();
        return 3U;
    }

    *data = Track_I2C_ReadByte(0U);
    Track_I2C_Stop();

    return 0U;
}

static uint8_t Track_LevelToBlack(uint8_t raw_level)
{
#if TRACK_BLACK_IS_1
    return (raw_level != 0U) ? 1U : 0U;
#else
    return (raw_level == 0U) ? 1U : 0U;
#endif
}

static uint8_t Track_ConvertToBlackBits(uint8_t raw_bits)
{
    uint8_t index;
    uint8_t black_bits = 0U;

    for (index = 0U; index < TRACK_SENSOR_COUNT; index++) {
        uint8_t raw_level =
            ((raw_bits &
                (uint8_t)(1U << g_track_raw_bit_for_sensor[index])) != 0U) ?
            1U : 0U;

        if (Track_LevelToBlack(raw_level) != 0U) {
            black_bits |= (uint8_t)(1U << index);
        }
    }

    return black_bits;
}

static void Track_UpdateError(uint8_t black_bits)
{
    uint8_t index;
    uint8_t active_count = 0U;
    int16_t weighted_sum = 0;

    for (index = 0U; index < TRACK_SENSOR_COUNT; index++) {
        if ((black_bits & (uint8_t)(1U << index)) != 0U) {
            weighted_sum += g_track_weight[index];
            active_count++;
        }
    }

    if (active_count == 0U) {
        g_track_lost = 1U;
        return;
    }

    g_track_lost = 0U;
    g_track_last_error = (int16_t)(weighted_sum / (int16_t) active_count);
}

static uint8_t Track_ReadRawI2C(uint8_t *raw_bits)
{
    return Track_I2C_ReadReg(TRACK_I2C_ADDR, TRACK_I2C_DATA_REG, raw_bits);
}

void Track_Init(void)
{
    g_track_raw_bits = 0xFFU;
    g_track_black_bits = 0U;
    g_track_lost = 1U;
    g_track_last_i2c_status = 0U;
    g_track_last_error = 0;
    g_track_read_count = 0U;
    g_track_i2c_error_count = 0U;
    g_track_last_diag_ms = 0U;

    IR_I2C_GPIO_Init();
    set_adjust_mode(0U);

    debug_printf("[TRACK8] soft i2c init SCL=PA26 SDA=PA24 addr=0x%02X data_reg=0x%02X valid=0x%02X\r\n",
        (unsigned int) TRACK_I2C_ADDR,
        (unsigned int) TRACK_I2C_DATA_REG,
        (unsigned int) TRACK_VALID_SENSOR_MASK);
}

void Track_DiagnosticUpdate(uint32_t now_ms)
{
#if (TRACK_DEBUG_LOG_MS > 0U)
    if ((uint32_t)(now_ms - g_track_last_diag_ms) >= TRACK_DEBUG_LOG_MS) {
        debug_printf("[TRACK8] raw=0x%02X black=0x%02X err=%d lost=%u i2c=%u errors=%lu reads=%lu\r\n",
            (unsigned int) g_track_raw_bits,
            (unsigned int) g_track_black_bits,
            (int) g_track_last_error,
            (unsigned int) g_track_lost,
            (unsigned int) g_track_last_i2c_status,
            (unsigned long) g_track_i2c_error_count,
            (unsigned long) g_track_read_count);
        g_track_last_diag_ms = now_ms;
    }
#else
    (void) now_ms;
#endif
}

void Track_SetUartBaud(uint32_t baud)
{
    (void) baud;
}

uint8_t Track_ReadBits(void)
{
    uint8_t raw_bits = 0xFFU;
    uint8_t status;

    status = Track_ReadRawI2C(&raw_bits);
    g_track_last_i2c_status = status;
    g_track_read_count++;

    if (status == 0U) {
        g_track_raw_bits = raw_bits;
        g_track_black_bits =
            (uint8_t)(Track_ConvertToBlackBits(raw_bits) &
                (uint8_t) TRACK_VALID_SENSOR_MASK);
        Track_UpdateError(g_track_black_bits);
    } else {
        g_track_i2c_error_count++;
        g_track_raw_bits = 0xFFU;
        g_track_black_bits = 0U;
        g_track_lost = 1U;
    }

    return g_track_black_bits;
}

void Track_ReadArray(uint8_t out[TRACK_SENSOR_COUNT])
{
    uint8_t index;
    uint8_t bits;

    if (out == NULL) {
        return;
    }

    bits = Track_ReadBits();
    for (index = 0U; index < TRACK_SENSOR_COUNT; index++) {
        out[index] = ((bits & (uint8_t)(1U << index)) != 0U) ? 1U : 0U;
    }
}

int16_t Track_GetLineError(void)
{
    return g_track_last_error;
}

uint8_t Track_IsLineLost(void)
{
    return g_track_lost;
}

uint8_t Track_GetRawBits(void)
{
    return g_track_raw_bits;
}

uint8_t Track_GetBlackBits(void)
{
    return g_track_black_bits;
}

int16_t Track_GetLastError(void)
{
    return g_track_last_error;
}

uint32_t Track_GetReadCount(void)
{
    return g_track_read_count;
}

uint8_t Track_GetLastI2cStatus(void)
{
    return g_track_last_i2c_status;
}

uint32_t Track_GetI2cErrorCount(void)
{
    return g_track_i2c_error_count;
}

void set_adjust_mode(u8 mode)
{
    (void) Track_I2C_WriteReg(TRACK_I2C_ADDR, TRACK_I2C_ADJUST_REG, mode);
}

void deal_IRdata(u8 *x1, u8 *x2, u8 *x3, u8 *x4,
    u8 *x5, u8 *x6, u8 *x7, u8 *x8)
{
    uint8_t raw_bits = 0xFFU;

    (void) Track_ReadRawI2C(&raw_bits);

    if (x1 != NULL) {
        *x1 = (uint8_t)((raw_bits >> 6U) & 0x01U);
    }
    if (x2 != NULL) {
        *x2 = (uint8_t)((raw_bits >> 5U) & 0x01U);
    }
    if (x3 != NULL) {
        *x3 = (uint8_t)((raw_bits >> 4U) & 0x01U);
    }
    if (x4 != NULL) {
        *x4 = (uint8_t)((raw_bits >> 3U) & 0x01U);
    }
    if (x5 != NULL) {
        *x5 = (uint8_t)((raw_bits >> 2U) & 0x01U);
    }
    if (x6 != NULL) {
        *x6 = (uint8_t)((raw_bits >> 1U) & 0x01U);
    }
    if (x7 != NULL) {
        *x7 = (uint8_t)(raw_bits & 0x01U);
    }
    if (x8 != NULL) {
        *x8 = (uint8_t)((raw_bits >> 7U) & 0x01U);
    }
}

uint8_t Track_GetLastRxByte(void)
{
    return g_track_raw_bits;
}

uint32_t Track_GetRxCount(void)
{
    return g_track_read_count;
}

uint32_t Track_GetFrameCount(void)
{
    return g_track_read_count;
}

uint32_t Track_GetCurrentBaud(void)
{
    return 0U;
}

uint8_t Track_IsBaudScanLocked(void)
{
    return 0U;
}
