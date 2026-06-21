#ifndef PLATFORM_TIME_H_
#define PLATFORM_TIME_H_

#include <stdint.h>

void platform_time_init(void);
uint32_t platform_time_ms(void);
void platform_delay_ms(uint32_t ms);

#endif /* PLATFORM_TIME_H_ */
