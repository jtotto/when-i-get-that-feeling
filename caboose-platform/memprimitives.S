.global mmu_enable_smp
.global mmu_set_ttbr
.global mmu_set_domain_access
.global mmu_flush_tlb
.global mmu_enable
.global cache_enable
.global dcache_min
.global dcache_invalidate_line
.global dcache_clean_line

/* ---- MMU control ---- */

/* See the ARM Architecture Reference Manual, Section 3.7.  All the MMU
 * registers are on the System Control coprocessor, CP15. */
mmu_enable_smp:
	/* We just need to set bit 6 of the ACTLR (c1 opcode 1) */
	mrc p15, 0, r0, c1, c0, 1
	orr r0, #0b1000000
	mrc p15, 0, r0, c1, c0, 1
	bx lr

mmu_set_ttbr:
	/* We're after coprocessor register 2, the TTBR. */
	mcr p15, 0, r0, c2, c0, 0
	bx lr

mmu_set_domain_access:
	/* We want coprocessor register 3, the domain access control register.  */
	lsl r0, #1
	lsl r1, r0
	mcr p15, 0, r1, c3, c0, 0
	bx lr

mmu_flush_tlb:
	/* See section 3.7.7 (TLB Functions) */
	/* "Invalidate entire unified TLB or both instruction and data TLBs." */
	mov r0, #0
	mcr p15, 0, r0, c8, c7, 0
	bx lr

mmu_enable:
	/* We want coprocessor register 1, bit 0 - the MMU enable bit! */
	mrc p15, 0, r0, c1, c0, 0
	orr r0, #1
	mcr p15, 0, r0, c1, c0, 0
	bx lr

/* ---- Cache control ---- */

#define SCTLR_I 0b1000000000000
#define SCTLR_C 0b0000000000100

cache_enable:
    /* We want to enable the I and C bits of the SCTLR. */
    mrc p15, 0, r0, c1, c0, 0
    orr r0, #SCTLR_I
    orr r0, #SCTLR_C
    mcr p15, 0, r0, c1, c0, 0
    bx lr

/* Cache Type Register documentation:
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438e/
 * BABDFDBG.html */

/* Return the number of bytes in the _smallest_ data cache line in the memory
 * hierarchy. */
dcache_min:
	mrc p15, 0, r0, c0, c0, 1
	lsr r0, #14  /* we want bits [19:16], but in bytes rather than words */
	and r0, #0x0000003C
	bx lr

/* We want to...
 * "Invalidate data cache line by VA (Virtual Address) to PoC (Point of
 * Coherency)."
 *
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0434b/
 * CIHGBCHB.html
 */
dcache_invalidate_line:
	mcr p15, 0, r0, c7, c6, 1
	bx lr

/* We want to...
 * "Clean data cache line by VA (Virtual Address) to PoC (Point of Coherency)."
 *
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0434b/
 * CIHGBCHB.html
 */
dcache_clean_line:
	mcr p15, 0, r0, c7, c10, 1
	bx lr
