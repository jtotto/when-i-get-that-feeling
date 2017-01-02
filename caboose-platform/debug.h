#ifndef CABOOSE_PLATFORM_DEBUG_H
#define CABOOSE_PLATFORM_DEBUG_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

void debug_printf(const char *fmt, ...);
void debug_vprintf(const char *fmt, va_list ap);
void debug_hexdump(const uint8_t *data, size_t len);

#endif
