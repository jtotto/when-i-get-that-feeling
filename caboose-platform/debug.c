#include <stdarg.h>

#include <mini-printf.h>

#include "debug.h"
#include "pl011-uart.h"

void debug_printf(const char *fmt, ...)
{
    int size;
    uint8_t buf[2048];
    va_list va;
    va_start(va, fmt);
    size = mini_vsnprintf((char *)buf, sizeof buf, fmt, va);
    va_end(va);

    for (int i = 0; i < size; i++) {
        uart0_putc(buf[i]);
    }

    uart0_putc('\n');
}

void debug_exception(uint32_t type, uint32_t lr)
{
    debug_printf("!!! Fatal exception !!! %u %x", type, lr);
}

void debug_gothere(void)
{
    debug_printf("Made it here?");
}
