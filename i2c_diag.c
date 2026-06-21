#include "i2c_diag.h"

#include "debug_uart.h"
#include "platform_i2c.h"
#include "ti_msp_dl_config.h"

volatile uint8_t i2c_scan_done;
volatile uint8_t i2c_found_addr[128];
volatile uint8_t i2c_found_count;
volatile uint8_t oled_found;
volatile uint8_t oled_found_addr;
volatile uint8_t mpu6050_found;
volatile uint8_t track_found;

static uint8_t i2c_addr_is_known(uint8_t addr)
{
    return ((addr == PLATFORM_I2C_ADDR_OLED) ||
            (addr == PLATFORM_I2C_ADDR_OLED_ALT) ||
            (addr == PLATFORM_I2C_ADDR_MPU6050)) ? 1U : 0U;
}

static void i2c_scan_impl(uint8_t verbose)
{
    uint8_t addr;

    i2c_scan_done = 0U;
    i2c_found_count = 0U;
    oled_found = 0U;
    oled_found_addr = 0U;
    mpu6050_found = 0U;
    track_found = 0U;

    if (verbose != 0U) {
        debug_printf("[I2C] config inst=I2C0 SDA=PA0 SCL=PA1 speed=%uHz\r\n",
            (unsigned int) I2C_0_BUS_SPEED_HZ);
        debug_print("[I2C] scan start\r\n");
    }

    for (addr = 0U; addr < 128U; addr++) {
        i2c_found_addr[addr] = 0U;
    }

    for (addr = I2C_SCAN_FIRST_ADDR; addr <= I2C_SCAN_LAST_ADDR; addr++) {
        platform_i2c_status_t status = platform_i2c_probe(addr);

        if (status == PLATFORM_I2C_OK) {
            i2c_found_addr[addr] = 1U;
            i2c_found_count++;

            if (verbose != 0U) {
                debug_printf("[I2C] scan found 0x%02X\r\n",
                    (unsigned int) addr);
            }

            if ((addr == PLATFORM_I2C_ADDR_OLED) ||
                (addr == PLATFORM_I2C_ADDR_OLED_ALT)) {
                oled_found = 1U;
                if (oled_found_addr == 0U) {
                    oled_found_addr = addr;
                }
            }

            if (addr == PLATFORM_I2C_ADDR_MPU6050) {
                mpu6050_found = 1U;
            }

        }
    }

    platform_i2c_recover_bus();
    if (verbose != 0U) {
        debug_print("[I2C] scan list:");
        for (addr = I2C_SCAN_FIRST_ADDR; addr <= I2C_SCAN_LAST_ADDR; addr++) {
            if (i2c_found_addr[addr] != 0U) {
                debug_printf(" 0x%02X", (unsigned int) addr);
            }
        }
        debug_print("\r\n");
        debug_printf("[I2C] scan done count=%u\r\n",
            (unsigned int) i2c_found_count);

        if (oled_found != 0U) {
            debug_printf("[I2C] OLED found addr=0x%02X\r\n",
                (unsigned int) oled_found_addr);
        } else {
            debug_print("[I2C] OLED not found, check VCC/GND/SCL/SDA/RST/address\r\n");
        }

        if (mpu6050_found != 0U) {
            debug_print("[I2C] MPU6050 found addr=0x68\r\n");
        } else {
            debug_print("[I2C] MPU6050 not found\r\n");
        }

        debug_print("[I2C] TRACK uses GPIO, not scanned on I2C\r\n");
    }

    i2c_scan_done = 1U;
}

void i2c_scan_clear(void)
{
    uint8_t addr;

    i2c_scan_done = 1U;
    i2c_found_count = 0U;
    oled_found = 0U;
    oled_found_addr = 0U;
    mpu6050_found = 0U;
    track_found = 0U;

    for (addr = 0U; addr < 128U; addr++) {
        i2c_found_addr[addr] = 0U;
    }
}

void i2c_scan(void)
{
    i2c_scan_impl(1U);
}

void i2c_scan_quiet(void)
{
    i2c_scan_impl(0U);
}

uint8_t i2c_find_first_unknown_addr(void)
{
    uint8_t addr;

    for (addr = I2C_SCAN_FIRST_ADDR; addr <= I2C_SCAN_LAST_ADDR; addr++) {
        if ((i2c_found_addr[addr] != 0U) &&
            (i2c_addr_is_known(addr) == 0U)) {
            return addr;
        }
    }

    return 0U;
}
