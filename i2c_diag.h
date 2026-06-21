#ifndef I2C_DIAG_H_
#define I2C_DIAG_H_

#include <stdint.h>

#define I2C_SCAN_FIRST_ADDR  (0x03U)
#define I2C_SCAN_LAST_ADDR   (0x77U)

extern volatile uint8_t i2c_scan_done;
extern volatile uint8_t i2c_found_addr[128];
extern volatile uint8_t i2c_found_count;
extern volatile uint8_t oled_found;
extern volatile uint8_t oled_found_addr;
extern volatile uint8_t mpu6050_found;
extern volatile uint8_t track_found;

void i2c_scan(void);
void i2c_scan_quiet(void);
void i2c_scan_clear(void);
uint8_t i2c_find_first_unknown_addr(void);

#endif /* I2C_DIAG_H_ */
