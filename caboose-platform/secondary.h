#ifndef CABOOSE_PLATFORM_SECONDARY_H
#define CABOOSE_PLATFORM_SECONDARY_H

#include <stdint.h>

void secondary_start(uint8_t coreno, void (*code)(void));

#endif
