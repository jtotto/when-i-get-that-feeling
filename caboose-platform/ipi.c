#include <caboose/platform.h>

#include "bcm2836.h"
#include "ipi.h"
#include "irq.h"

void (*ipi_handlers[32])(void);

uint8_t *ipi_init(uint8_t *pool)
{
    /* Following Circle's example, we'll use core 0's mailbox 0 for IPIs.  Other
     * cores set the bit corresponding to the IPI they wish to deliver in
     * mailbox 0, which will assert the core 0 IRQ. */

    /* Connect core 0's mailbox 0 interrupt to the IRQ line. (see section 4.7 of
     * the BCM2836 datasheet) */
    volatile uint32_t *core0_mailbox_intctl =
        (uint32_t *)ARM_LOCAL_MAILBOX_INT_CONTROL0;
    *core0_mailbox_intctl = 1 << 0;

    return pool;
}

void ipi_register(uint8_t ipi, void (*handler)(void))
{
    ASSERT(!ipi_handlers[ipi]);
    ipi_handlers[ipi] = handler;
}

void ipi_deliver(uint8_t ipi)
{
    volatile uint32_t *core0_mailbox0_set0 =
        (uint32_t *)ARM_LOCAL_MAILBOX0_SET0;
    *core0_mailbox0_set0 = 1 << ipi;
}

bool ipi_pending(uint8_t ipi)
{
    volatile uint32_t *core0_mailbox0_clr0 =
        (uint32_t *)ARM_LOCAL_MAILBOX0_CLR0;
    return !!(*core0_mailbox0_clr0 & (1 << ipi));
}

void ipi_service(void)
{
    volatile uint32_t *core0_mailbox0_clr0 =
        (uint32_t *)ARM_LOCAL_MAILBOX0_CLR0;
    while (true) {
        uint32_t val = *core0_mailbox0_clr0;
        if (!val) {
            break;
        }

        do {
            uint8_t ipi = __builtin_ctz(val);
            ASSERT(ipi_handlers[ipi]);
            ipi_handlers[ipi]();
            *core0_mailbox0_clr0 = 1 << ipi;
            val &= ~(1 << ipi);
        } while (val);
    }
}
