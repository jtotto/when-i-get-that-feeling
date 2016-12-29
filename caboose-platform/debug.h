#ifndef CABOOSE_PLATFORM_DEBUG_H
#define CABOOSE_PLATFORM_DEBUG_H

#include <stddef.h>
#include <stdint.h>

void debug_printf(const char *fmt, ...);
void debug_hexdump(uint8_t *data, size_t len);

#endif
