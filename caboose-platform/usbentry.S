.global fiq_except
.global usb_start

#include "coreinit.inc"

#define FIQ_MODE 0b10001
#define SVC_MODE 0b10011

usb_start:
    /* Put the ARM in SVC mode so we have the exception semantics we're
     * expecting. */
    safe_svcmode_maskall r0

    /* Set up the SVC stack (which we'll really just be using to run the USB
     * initialization code). */
    ldr r0, =usb_svc_stack
    ldr sp, [r0]

    /* Switch to FIQ mode to set up the stack pointer. */
    cps #FIQ_MODE
    ldr r0, =usb_fiq_stack
    ldr sp, [r0]

    /* And back to SVC to wait. */
    cps #SVC_MODE

    /* This wasn't clear to me from the ARM ARM, but I've learned the hard way
     * that there's a VBAR per core (which is actually kind of nice).  We're
     * fine with using the same vector table as the main core, though, so we'll
     * just get that set up here. */
    ldr r0, =vector_table
    mcr p15, 0, r0, c12, c0, 0

    /* Enable hardware floating point. */
    enable_vfp r0

    /* Actually enable FIQs.  USPi's initialization depends on receiving USB
     * interrupts - it initiates a lot of synchronous transfers while
     * configuring devices, which it implements as a spin-loop testing a bit
     * that gets cleared by its interrupt handler, so interrupts need to be
     * available _before_ beginning the library initialization. */
    cpsie f

    /* Initialize USPi! */
    bl usb_uspi_init

wait_fiq:
    /* Wait in a tight loop for USB FIQs. */
    b wait_fiq

fiq_except:
    /* Fix up the link register. */
    sub lr, #4

    /* Stash the non-banked registers on our stack (the FIQ stack).  Note that
     * we need to do this even though at steady state there isn't any context to
     * preserve because we're forced to run the USPi initialization routine on
     * this core, and it requires that interrupts be enabled in order to
     * complete. */
    stmfd sp!, {r0-r7, lr}
    /* Fix up sp's alignment. */
    sub sp, #4

    /* Run our handler function. */
    bl usb_handle_fiq

    /* Return from the exception. */
    add sp, #4
    ldmia sp!, {r0-r7, pc}^
