.global engine
.global start
.global vector_table

start:
    /* Set up the kernel stack, which will remain in the SVC-mode sp forever. */
    ldr sp, =kernel_stack

    /* Point the VBAR at our vector table. */
    ldr r0, =vector_table
    mcr p15, 0, r0, c12, c0, 0

    /* Call platform_init() with the start of the free memory pool. */
    ldr r0, =pool_begin
    b platform_init

/* Bits 0-4 of the VBAR are reserved, so we need the vector table to be 32-byte
 * aligned.  Helpful comments courtesy of Embedded Xinu. */
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

reset_addr:     .word dead
undef_addr:     .word dead
svc_addr:       .word software_except
prefetch_addr:  .word dead
abort_addr:     .word dead
reserved_addr:  .word dead
irq_addr:       .word irq_except
fiq_addr:       .word dead

dead:
    b dead
