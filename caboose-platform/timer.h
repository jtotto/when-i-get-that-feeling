#ifndef CABOOSE_PLATFORM_TIMER_H
#define CABOOSE_PLATFORM_TIMER_H

#include <stdint.h>

uint8_t *timer_init(uint8_t *pool);

uint32_t timer_read(void);

#endif
