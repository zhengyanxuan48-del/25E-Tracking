#ifndef PLATFORM_I2C_H_
#define PLATFORM_I2C_H_

#include <stdbool.h>
#include <stdint.h>

/*
 * Shared MSPM0G3507 I2C platform layer.
 *
 * Address convention: all APIs take 7-bit I2C target addresses.
 * Diagnostic scans may cover 0x03..0x77. Normal devices should still use
 * standard non-reserved 7-bit addresses.
 * Examples:
 *   MPU6050: 0x68
 *   OLED SSD1306/SH1106: 0x3C, not STM32-style 0x78
 */

#define PLATFORM_I2C_ADDR_MPU6050  (0x68U)
#define PLATFORM_I2C_ADDR_OLED     (0x3CU)
#define PLATFORM_I2C_ADDR_OLED_ALT (0x3DU)

typedef enum {
    PLATFORM_I2C_OK = 0,
    PLATFORM_I2C_ERROR = -1,
    PLATFORM_I2C_TIMEOUT = -2,
    PLATFORM_I2C_BAD_PARAM = -3,
} platform_i2c_status_t;

typedef struct {
    uint8_t sda_high;
    uint8_t scl_high;
    uint32_t controller_status;
    uint32_t raw_interrupt;
} platform_i2c_bus_state_t;

void platform_i2c_init(void);
void platform_i2c_recover_bus(void);
void platform_i2c_bus_recover(void);
void platform_i2c_get_bus_state(platform_i2c_bus_state_t *state);

platform_i2c_status_t platform_i2c_probe(uint8_t addr_7bit);
platform_i2c_status_t platform_i2c_probe_write_byte(
    uint8_t addr_7bit, uint8_t value);
platform_i2c_status_t platform_i2c_probe_read(uint8_t addr_7bit);

platform_i2c_status_t platform_i2c_write(
    uint8_t addr_7bit, const uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_write_bytes(
    uint8_t addr_7bit, const uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_write_control_bytes(
    uint8_t addr_7bit, uint8_t control, const uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_read(
    uint8_t addr_7bit, uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_read_bytes(
    uint8_t addr_7bit, uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_write_reg(
    uint8_t addr_7bit, uint8_t reg, const uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_read_reg(
    uint8_t addr_7bit, uint8_t reg, uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_read_regs(
    uint8_t addr_7bit, uint8_t reg, uint8_t *data, uint16_t len);

platform_i2c_status_t platform_i2c_write_u8(
    uint8_t addr_7bit, uint8_t reg, uint8_t value);

platform_i2c_status_t platform_i2c_read_u8(
    uint8_t addr_7bit, uint8_t reg, uint8_t *value);

#endif /* PLATFORM_I2C_H_ */
