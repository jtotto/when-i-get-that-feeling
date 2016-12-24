#ifndef CABOOSE_PLATFORM_IRQ_H
#define CABOOSE_PLATFORM_IRQ_H

#include <stdint.h>

uint8_t *irq_init(uint8_t *pool);

void irq_enable(uint8_t irq);
void irq_disable(uint8_t irq);

void irq_register(uint8_t irq, void (*handler)(void));

#endif
