#include <caboose/platform.h>

#include "irq.h"

void (*irq_handlers[64])(void);

/* N.B. The IRQ handler itself is installed by start.S with the vector table */
uint8_t *irq_init(uint8_t *pool)
{
    /* Nothing to do for now. */

    return pool;
}

void irq_enable(uint8_t irq)
{
    /* XXX */
}

void irq_register(uint8_t irq, void (*handler)(void))
{
    ASSERT(handler);
    ASSERT(!irq_handlers[irq]);
    irq_handlers[irq] = handler;

    irq_enable(irq);
}

void irq_service(void)
{
    /* XXX */
}
