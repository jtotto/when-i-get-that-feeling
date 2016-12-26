#include <caboose/events.h>

#include "bcm2835.h"
#include "bcm2835int.h"
#include "debug.h"
#include "irq.h"
#include "platform-events.h"
#include "timer.h"

/* This is a simple driver for the system timer described in Chapter 12 of the
 * BCM2835 peripherals document.  Lots of inspiration taken from the Circle
 * CTimer. */

#define CLOCK_FREQ 1000000 /* RPi baremetal forum community wisdom */

#define TICK_HZ 1   /* for now we're not using this for anything serious */
#define COMPARE_INCREMENT (CLOCK_FREQ / TICK_HZ)

struct systimerregs {
    uint32_t cs;
    uint32_t clo;
    uint32_t chi;
    uint32_t c0;
    uint32_t c1;
    uint32_t c2;
    uint32_t c3;
} __packed;

static void timer_irq_handler(void)
{
    volatile struct systimerregs *timer =
        (struct systimerregs *)ARM_SYSTIMER_BASE;

    /* Compute the new compare value needed to get an interrupt at our next
     * desired time. */
    timer->c3 = timer->clo + COMPARE_INCREMENT;

    /* "Write a one to the relevant bit to clear the match detect status bit and
     * the corresponding interrupt request line" */
    timer->cs = 1 << 3;

    event_deliver(TIMER_EVENTID, 0xcab005e);
}

uint8_t *timer_init(uint8_t *pool)
{
    volatile struct systimerregs *timer =
        (struct systimerregs *)ARM_SYSTIMER_BASE;

    /* The systimer is always free-running - all we need to do here and in the
     * ISR is keep our chosen timer's match register up to date.  We choose
     * timer 3 because "the GPU uses timers 0 and 2. 3 is reserved for linux, so
     * would be most suitable for a bare metal OS" - more community wisdom */
    timer->c3 = timer->clo + COMPARE_INCREMENT;

    irq_register(ARM_IRQ_TIMER3, timer_irq_handler);
    
    return pool;
}

uint32_t timer_read(void)
{
    volatile struct systimerregs *timer =
        (struct systimerregs *)ARM_SYSTIMER_BASE;

    debug_printf("match %u control %u", timer->c3, timer->cs);
    return timer->clo;
}
