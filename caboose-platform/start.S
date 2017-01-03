.global engine
.global start
.global vector_table
.global secondary_null_start

#include "coreinit.inc"

start:
    /* Put the ARM in SVC mode. */
    safe_svcmode_maskall r0

    /* Set up the kernel stack, which will remain in the SVC-mode sp forever. */
    ldr sp, =kernel_stack

    /* Point the VBAR at our vector table. */
    ldr r0, =vector_table
    mcr p15, 0, r0, c12, c0, 0

    /* Enable hardware floating point. */
    enable_vfp r0

    /* Call platform_init() with the start of the free memory pool. */
    ldr r0, =pool_begin
    b platform_init

/* Bits 0-4 of the VBAR are reserved, so we need the vector table to be 32-byte
 * aligned.  Helpful descriptions courtesy of Embedded Xinu. */
.balign 32
vector_table:
    ldr pc, reset_addr    /* Reset handler */
    ldr pc, undef_addr    /* Undefined instruction handler */
    ldr pc, svc_addr      /* Software interrupt handler */
    ldr pc, prefetch_addr /* Prefetch abort handler */
    ldr pc, abort_addr    /* Data abort handler */
    ldr pc, reserved_addr /* Reserved */
    ldr pc, irq_addr      /* IRQ (Interrupt request) handler */
    ldr pc, fiq_addr      /* FIQ (Fast interrupt request) handler */

reset_addr:     .word dead1
undef_addr:     .word dead2
svc_addr:       .word software_except
prefetch_addr:  .word dead3
abort_addr:     .word dead4
reserved_addr:  .word dead5
irq_addr:       .word irq_except
fiq_addr:       .word fiq_except

reallydead:
    b reallydead

.macro dead type
dead\type:
    mov sp, #0x8000
    mov r0, #\type
    mov r1, lr
    bl debug_exception
    b reallydead
 .endm

dead 1
dead 2
dead 3
dead 4
dead 5
dead 6

secondary_null_start:
    /* Just sleep forever.  We do this so the core stops spamming the bus with
     * attempted reads of its start address. */
    wfi
    b secondary_null_start
