#ifndef DEBUG_UART_H_
#define DEBUG_UART_H_

#include <stdint.h>

void debug_print(const char *s);
void debug_printf(const char *fmt, ...);
void debug_print_hex(uint8_t value);

#endif /* DEBUG_UART_H_ */
