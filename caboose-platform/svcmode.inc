#ifndef CABOOSE_PLATFORM_SVCMODE_INC
#define CABOOSE_PLATFORM_SVCMODE_INC

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

#endif
