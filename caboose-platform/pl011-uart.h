#ifndef CABOOSE_PLATFORM_PL011_UART_H
#define CABOOSE_PLATFORM_PL011_UART_H

#include <stdint.h>

uint8_t *uart0_init(uint8_t *pool);
void uart0_putc(uint8_t c);

#endif
