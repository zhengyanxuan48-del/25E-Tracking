#include "debug_uart.h"

#include "ti_msp_dl_config.h"
#include <stdarg.h>
#include <stdio.h>

#define DEBUG_PRINTF_BUFFER_SIZE  128

static void debug_putc(char c)
{
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) c);
}

void debug_print(const char *s)
{
    if (s == NULL) {
        return;
    }

    while (*s != '\0') {
        debug_putc(*s);
        s++;
    }
}

void debug_printf(const char *fmt, ...)
{
    char buffer[DEBUG_PRINTF_BUFFER_SIZE];
    va_list args;
    int length;

    if (fmt == NULL) {
        return;
    }

    va_start(args, fmt);
    length = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (length > 0) {
        debug_print(buffer);
    }
}

void debug_print_hex(uint8_t value)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    debug_print("0x");
    debug_putc(hex_digits[(value >> 4) & 0x0F]);
    debug_putc(hex_digits[value & 0x0F]);
}
