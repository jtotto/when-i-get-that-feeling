#include <caboose/platform.h>
#include <caboose/util.h>

#include "bcm2835.h"
#include "bcm2835int.h"
#include "bcm2836.h"
#include "debug.h"
#include "ipi.h"
#include "irq.h"
#include "util.h"

/* This is the interrupt-control register interface documented in Chapter 7 of
 * the original BCM2835 datasheet. */
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

#define FIQCTL_ENABLE (1 << 7)

/* This is the "ARM control logic" register interface documented by the BCM2836
 * local peripherals datasheet for the RPi2. */
struct bcm2836regs {
    LEADPAD(ARM_LOCAL_BASE, ARM_LOCAL_GPU_INT_ROUTING);
    uint32_t gpuintrouting;
} __packed;

#define GPU_INT_ROUTING_IRQ_SHIFT 0
#define GPU_INT_ROUTING_FIQ_SHIFT 2

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
    /* Things that we _don't_ need to take care of here: the IRQ/FIQ handlers
     * are installed as part of the vector table setup in the boot stub, and
     * interrupts are enabled in the ARM implicitly whenever we resume user
     * code. */

    /* Route IRQs and FIQs to the cores specified in the configuration. */
    volatile struct bcm2836regs *bcm2836 =
        (struct bcm2836regs *)ARM_LOCAL_BASE;
    bcm2836->gpuintrouting = (CONFIG_IRQ_CORE << GPU_INT_ROUTING_IRQ_SHIFT)
                             | (CONFIG_FIQ_CORE << GPU_INT_ROUTING_FIQ_SHIFT);

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

void irq_disable(uint8_t irq)
{
    volatile struct irqregs *irqregs = (struct irqregs *)ARM_IC_BASE;
    if (irq < ARM_IRQS_PER_REG) {
        irqregs->disable1 = 1 << irq;
    } else {
        irqregs->disable2 = 1 << (irq - ARM_IRQS_PER_REG);
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

    /* It's possible that the IRQ line is being asserted by the core local
     * mailbox we're using to implement IPIs.  That mailbox doesn't have a bit
     * in any of the pending registers, so we'll have to check it directly. */
    ipi_service();

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

void fiq_register(uint8_t irq)
{
    volatile struct irqregs *irqregs = (struct irqregs *)ARM_IC_BASE;

    ASSERT(!irq_handlers[irq]);
    irqregs->fiqctl = FIQCTL_ENABLE | irq;

    /* Explicitly disable this interrupt as an IRQ so we don't trip over it in
     * the pending register. */
    irq_disable(irq);
}
