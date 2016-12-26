#include <caboose/platform.h>
#include <caboose/util.h>

#include "bcm2835.h"
#include "bcm2835int.h"
#include "debug.h"
#include "irq.h"
#include "util.h"

struct irqregs {
    LEADPAD(ARM_IC_BASE, ARM_IC_IRQ_BASIC_PENDING);
    uint32_t pending_basic;
    uint32_t pending1;
    uint32_t pending2;
    uint32_t fiqctl;
    uint32_t enable1;
    uint32_t enable2;
    uint32_t enable_basic;
    uint32_t disable1;
    uint32_t disable2;
    uint32_t disable_basic;
} __packed;

void dump_irqregs(void)
{
    volatile struct irqregs *irqregs = (struct irqregs *)ARM_IC_BASE;
    debug_printf("irq regdump %u %u %u %u %u %u",
                 irqregs->pending1,
                 irqregs->pending2,
                 irqregs->enable1,
                 irqregs->enable2,
                 irqregs->disable1,
                 irqregs->disable2);
}

void (*irq_handlers[64])(void);

uint8_t *irq_init(uint8_t *pool)
{
    /* Nothing to do for now - the IRQ handler is installed as part of the
     * vector table setup in the boot stub, and interrupts are enabled
     * in the ARM implicitly whenever we resume user code. */
    return pool;
}

void irq_enable(uint8_t irq)
{
    volatile struct irqregs *irqregs = (struct irqregs *)ARM_IC_BASE;
    if (irq < ARM_IRQS_PER_REG) {
        irqregs->enable1 = 1 << irq;
    } else {
        irqregs->enable2 = 1 << (irq - ARM_IRQS_PER_REG);
    }
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
    volatile struct irqregs *irqregs = (struct irqregs *)ARM_IC_BASE;

    /* The logic here is a bit tangly, but the idea is that we don't want to
     * call it quits until we're really sure there aren't any IRQs asserted - in
     * particular, we don't want to attempt to return until we've checked that
     * more IRQs haven't become pending since we started servicing the first
     * one. */
    while (true) {
        uint32_t pending1 = irqregs->pending1;
        uint32_t pending2 = irqregs->pending2;

        if (!pending1 && !pending2) {
            break;
        }

        while (pending1) {
            int irq = __builtin_ctz(pending1);
            ASSERT(irq_handlers[irq]);
            irq_handlers[irq]();

            pending1 &= ~(1 << irq);
        }

        while (pending2) {
            int irqbit = __builtin_ctz(pending2);
            int irq = ARM_IRQS_PER_REG + irqbit;
            ASSERT(irq_handlers[irq]);
            irq_handlers[irq]();

            pending2 &= ~(1 << irqbit);
        }
    }
}
