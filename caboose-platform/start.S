.global engine
.global start
.global vector_table

/*
 * "Helper macro to enter SVC mode cleanly and mask interrupts. reg is
 * a scratch register for the macro to overwrite.
 *
 * This macro is intended for forcing the CPU into SVC mode at boot time.
 * you cannot return to the original mode."
 *
 * Reproduced verbatim from startup.S rsta2's Circle, which in turn is credited
 * to arch/arm/include/asm/assembler.h in Linux.  We use this here because the
 * post-October 2015 boot firmware's per core boot stub puts the ARM in HYP
 * mode, while we'd rather program in SVC mode.
 */
    .macro safe_svcmode_maskall reg:req

    mrs \reg , cpsr
    eor \reg, \reg, #0x1A       /* test for HYP mode */
    tst \reg, #0x1F
    bic \reg , \reg , #0x1F     /* clear mode bits */
    orr \reg , \reg , #0xC0 | 0x13  /* mask IRQ/FIQ bits and set SVC mode */
    bne 1f              /* branch if not HYP mode */
    orr \reg, \reg, #0x100      /* mask Abort bit */
    adr lr, 2f
    msr spsr_cxsf, \reg
    .word   0xE12EF30E          /* msr ELR_hyp, lr */
    .word   0xE160006E          /* eret */
1:  msr cpsr_c, \reg
2:

    .endm

start:
    /* Put the ARM in SVC mode. */
    safe_svcmode_maskall r0

    /* Set up the kernel stack, which will remain in the SVC-mode sp forever. */
    ldr sp, =kernel_stack

    /* Point the VBAR at our vector table. */
    ldr r0, =vector_table
    mcr p15, 0, r0, c12, c0, 0

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

