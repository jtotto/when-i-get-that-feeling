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

void debug_hexdump(uint8_t *data, size_t len)
{
    debug_printf("Begin hex dump");
    while (len >= 16) {
        debug_printf("%02x%02x%02x%02x "
                     "%02x%02x%02x%02x "
                     "%02x%02x%02x%02x "
                     "%02x%02x%02x%02x",
                     data[0],
                     data[1],
                     data[2],
                     data[3],
                     data[4],
                     data[5],
                     data[6],
                     data[7],
                     data[8],
                     data[9],
                     data[10],
                     data[11],
                     data[12],
                     data[13],
                     data[14],
                     data[15]);
        data += 16;
        len -= 16;
    }
}
