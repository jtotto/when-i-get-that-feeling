#include <caboose/platform.h>

#include "irq.h"

/* The vexpress-a15 has a standard ARM GIC.  This driver is my attempt at putting
 * together the absolute bare-minimum required to get a timer interrupt working
 * through the GIC, because my actual intended target platform doesn't have one.
 * I referred mostly to the Altera "Using the ARM Generic Interrupt Controller"
 * documentation for initialization, and lifted the ISR code from Linux.
 *
 * In vexpress.c, the interrupt initialization code notes that interrupts 0-31
 * are chip internal, and consequently that QEMU interrupt N is actually
 * physical interrupt N + 32.  I wanted to be able to refer to IRQs by
 * their number in vexpress.c, so I translate between the QEMU numbering and the
 * physical numbering throughout as required. */

#define GIC_BASE 0x2c000000

#define GIC_DISTRIBUTOR_BASE (GIC_BASE + 0x1000)
#define DCONTROL 0x0
#define DSETENABLE1 0x104
#define DPROCESSOR_TARGET32 0x820

#define GIC_CPU_BASE (GIC_BASE + 0x2000)
#define CCONTROL 0x0
#define CPRIORITY_MASK 0x4
#define CINTERRUPT_ACKNOWLEDGE 0xc
#define CEND_OF_INTERRUPT 0x10

/* We only support interrupts 32-63 - see above. */
void (*irq_handlers[32])(void);

/* N.B. The IRQ handler itself is installed by start.S with the vector table */
uint8_t *irq_init(uint8_t *pool)
{
    volatile uint32_t *priority_mask, *cpu_control, *distributor_control;
    priority_mask = (uint32_t *)(GIC_CPU_BASE + CPRIORITY_MASK);
    cpu_control = (uint32_t *)(GIC_CPU_BASE + CCONTROL);
    distributor_control = (uint32_t *)(GIC_DISTRIBUTOR_BASE + DCONTROL);

	/* Enable all priorities of interrupts in the priority mask register. */
    *priority_mask = 0xffff;

    /* Enable the signalling of interrupts to the CPU interface. */
    *cpu_control = 0x1;

    /* Enable the forwarding of pendig interrupts from the distributor to CPU
     * intefaces. */
    *distributor_control = 0x1;

    return pool;
}

void irq_enable(uint8_t irq)
{
    volatile uint32_t *distributor_setenable1;
    volatile uint8_t *distributor_target;
    distributor_setenable1 = (uint32_t *)(GIC_DISTRIBUTOR_BASE + DSETENABLE1);
    distributor_target = (uint8_t *)(GIC_DISTRIBUTOR_BASE
                                     + DPROCESSOR_TARGET32
                                     + irq);

    /* First, enable the interrupt on the distributor. */
    *distributor_setenable1 |= 1 << irq;

    /* Target that interrupt at CPU 0. */
    *distributor_target = 1 << 0;
}

void irq_register(uint8_t irq, void (*handler)(void))
{
    ASSERT(handler);
    ASSERT(!irq_handlers[irq]);
    irq_handlers[irq] = handler;

    irq_enable(irq);
}

/* Based on gic_handle_irq() in Linux's drivers/irqchip/irq-gic.c */
void irq_service(void)
{
    volatile uint32_t *cpu_acknowledge, *cpu_eoi;

    cpu_acknowledge = (uint32_t *)(GIC_CPU_BASE + CINTERRUPT_ACKNOWLEDGE);
    cpu_eoi = (uint32_t *)(GIC_CPU_BASE + CEND_OF_INTERRUPT);

    while (true) {
        uint32_t irq = *cpu_acknowledge;
        if (irq == 0x3ff) {
            break;
        }

        /* We only support interrupts 32-63. */
        ASSERT(irq >= 32 && irq < 64);
        ASSERT(irq_handlers[irq - 32]);
        irq_handlers[irq - 32]();

        *cpu_eoi = irq;
    }
}
