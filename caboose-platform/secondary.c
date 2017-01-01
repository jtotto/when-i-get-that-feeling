#include <caboose/platform.h>

#include "bcm2836.h"
#include "secondary.h"

/* The closest thing to official documentation when it comes to bringing up the
 * secondary cores is a comment on a thread [1] in the Raspberry Pi forum by the
 * Raspberry Pi Foundation engineer @jdb, who says: "With the standard firmware
 * bootloader, a small stub is copied to the ARM reset vector address which:
 * a) brings up core 0 which then starts booting the Linux kernel
 * b) puts cores 1-3 in a state where, upon receiving an event, they read
 * Core[n]_mailbox3 and if this is nonzero, they begin executing from the
 * address pointed to by the mailbox (the entry point for additional cores in
 * the Linux kernel)."
 *
 * What's a mailbox, though?  The BCM2836 datasheet has this to say: "A mailbox
 * is a 32-bit wide write-bits-high-to-set and write-bits-high-to-clear
 * register. The write-set register addresses are write only. The write-clear
 * register addresses can be read as well. A mailbox generates and interrupt as
 * long as it is non-zero."  Essentially, mailboxes are just registers that can
 * be read and written by both the ARM and the VideoCore GPU that have
 * interrupts attached to them that can be used for communication between the
 * two (and for communication between the cores as well).
 *
 * The Raspberry Pi firmware repository has a wiki [2] that describes the mailbox
 * programming interface, as well as the protocol for using the mailboxes to
 * request services from the VideoCore.  However, here we're not concerned with
 * the VideoCore - instead, we're building on the groundwork laid by the
 * firmware boot stub for the secondary cores to get some other code of our
 * choosing running on them.  @jdb's comment indicates that all we need to get
 * code of our choosing running on core n is write to core n's mailbox 3 with an
 * address to execute from.
 *
 * [1] https://www.raspberrypi.org/forums/viewtopic.php?p=789655#p789655 
 * [2] https://github.com/raspberrypi/firmware/wiki/Mailboxes
 * */

void secondary_null_start(void);

void secondary_start(uint8_t coreno, void (*code)(void))
{
    /* The register layout is such that we could use arithmetic to compute the
     * address of core n's mailbox registers, but I'd rather they be explicit
     * and use the macros from the Linux header file. */
    uint32_t set3addr, clr3addr;
    switch (coreno)
    {
        /* We don't permit core 0 because that's the boot core. */
        case 1:
            set3addr = ARM_LOCAL_MAILBOX3_SET1;
            clr3addr = ARM_LOCAL_MAILBOX3_CLR1;
            break;
        case 2:
            set3addr = ARM_LOCAL_MAILBOX3_SET2;
            clr3addr = ARM_LOCAL_MAILBOX3_CLR2;
            break;
        case 3:
            set3addr = ARM_LOCAL_MAILBOX3_SET3;
            clr3addr = ARM_LOCAL_MAILBOX3_CLR3;
            break;
        default:
            ASSERT(false);
    }

    code = code ?: secondary_null_start;

    volatile uint32_t *set3 = (uint32_t *)set3addr;
    volatile uint32_t *clr3 = (uint32_t *)clr3addr;

    /* We'll follow Circle's lead here and wait until Mailbox 3 is clear,
     * presumably to ensure that the boot stub has had a chance to sort itself
     * out and clear the mailbox for writing. */
    while (*clr3) {
        /* wait */
    }

    /* Now that we know that the entire mailbox register is clear, we can assign
     * the address we want the core to start booting from by writing it to the
     * set register, which will set all of the bits that are set in the address
     * and leave clear all of the bits that aren't. */
    *set3 = (uint32_t)code;
}
