#include <stddef.h>

#include <caboose/util.h>

#include "barriers.h"
#include "bcm2835.h"
#include "mmu.h"

/* "System software often requires invalidation of a range of addresses that
 * might be present in multiple processors. This is accomplished with a loop of
 * invalidate cache by MVA CP15 operations that step through the address space
 * in cache line-sized strides. For code to be portable across all ARMv7-A
 * architecture-compliant devices, system software queries the CP15 Cache Type
 * Register to obtain the stride size, see Cache Type Register for more
 * information."
 * 
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0438e/
 * BABHAEIF.html
 */

/* Cache control primitives. */
void cache_enable(void);
uint32_t dcache_min(void);
void dcache_invalidate_line(void *addr);
void dcache_clean_line(void *addr);
void dcache_clean_and_invalidate_line(void *addr);

/* XXX We assume this doesn't vary across cores and can be shared globally. */
int cacheline_len;

/* Let the optimizer fold all this away. */
static void dcachectl_do_range(void *addr, int len, void (*op)(void *addr))
{
    /* How many cache lines does this range span? */
    size_t lines = (len + cacheline_len - 1) / cacheline_len;
    char *line;
    size_t i;
    for (i = 0, line = addr; i < lines; i++, line += cacheline_len) {
        op(line);
    }
}

void dcachectl_clean_range(void *addr, int len)
{
    dcachectl_do_range(addr, len, dcache_clean_line);
}

void dcachectl_invalidate_range(void *addr, int len)
{
    dcachectl_do_range(addr, len, dcache_invalidate_line);
}

void dcachectl_clean_and_invalidate_range(void *addr, int len)
{
    dcachectl_do_range(addr, len, dcache_clean_and_invalidate_line);
}

void sys_Clean(void *addr, int len)
{
    dsb();
    dcachectl_clean_range(addr, len);
}

void sys_Invalidate(void *addr, int len)
{
    dcachectl_invalidate_range(addr, len);
    dsb();
}

void sys_CleanAndInvalidate(void *addr, int len)
{
    dsb();
    dcachectl_clean_and_invalidate_range(addr, len);
    dsb();
}

/* Each top-level page table entry describes the configuration of a 1MB
 * 'section' of address space.  We're going to set up an identity mapping for
 * addresses from the perspective of the CPU - that is, we'll map each virtual
 * section directly to its corresponding physical section.  Then, we'll
 * partition the address space in two - physical memory addresses, and mapped
 * peripheral addresses - so that we can set the bufferability/cacheability
 * attributes of each partition appropriately. */
#define PAGE_TABLE_ENTRIES 4096
#define PAGE_TABLE_PHYSICAL_ENTRIES (ARM_IO_BASE / (1 << 20))
#define PAGE_TABLE_DEVICE_ENTRIES \
    (PAGE_TABLE_ENTRIES - PAGE_TABLE_PHYSICAL_ENTRIES)
#define PAGE_TABLE_SIZE (4*PAGE_TABLE_ENTRIES)

#define MMU_DOMAIN_ACCESS_MANAGER 0b11

/* Section 3.3.4 of the ARM Architecture Reference Manual */
struct mmu_section_descriptor {
    uint32_t type : 2;          /* 0b10 for section descriptors */
    uint32_t bufferable : 1;    /* does what you think */
    uint32_t cacheable : 1;     /* does what you think */
    uint32_t impl1 : 1;         /* don't touch, should be 0 */
    uint32_t domain : 4;        /* see section 3.5 */
    uint32_t impl2 : 1;         /* don't touch, should be 0 */
    uint32_t access : 2;        /* see section 3.4 */
    uint32_t impl3 : 8;         /* don't touch, should be 0 */
    uint32_t baseaddr : 12;     /* base address of the described section */
};

/* MMU control primitives. */
void mmu_enable_smp(void);
void mmu_set_ttbr(volatile struct mmu_section_descriptor *pagetable);
void mmu_set_domain_access(uint8_t domain, uint8_t access);
void mmu_flush_tlb(void);
void mmu_enable(void);

uint8_t *mmu_init(uint8_t *pool)
{
    cacheline_len = dcache_min();

    mmu_enable_smp();

    /* The page table needs to be 16K-aligned. */
    uint32_t baseaddr = ALIGN((uint32_t)pool, (1 << 14));
    volatile struct mmu_section_descriptor *pagetable =
        (struct mmu_section_descriptor *)baseaddr;

    int i;
    for (i = 0; i < PAGE_TABLE_PHYSICAL_ENTRIES; i++) {
        pagetable[i] = (struct mmu_section_descriptor) {
            .type = 0b10,
            .bufferable = 1,
            .cacheable = 1,
            .impl1 = 0,
            .domain = 0,
            .impl2 = 0,
            .access = 0b10, /* system access only */
            .impl3 = 0,
            .baseaddr = i
        };
    }

    for (i = PAGE_TABLE_PHYSICAL_ENTRIES; i < PAGE_TABLE_ENTRIES; i++) {

        pagetable[i] = (struct mmu_section_descriptor) {
            .type = 0b10,
            /* strongly-ordered memory accesses, please */
            .bufferable = 0,
            .cacheable = 0,
            .impl1 = 0,
            .domain = 0,
            .impl2 = 0,
            .access = 0b10,
            .impl3 = 0,
            .baseaddr = i
        };
    }

    /* Make sure that the table is fully coherent in main memory before
     * proceeding to use it. */
    dcachectl_clean_range((void *)pagetable, PAGE_TABLE_SIZE);
    dsb();
    isb();

    /* Configure the Translation Table Base Register 0 to point to our
     * top-level page table. */
    mmu_set_ttbr(pagetable);

    /* Configure the domain used in all of the page table entries, domain 0, to
     * have 'manager' access - that is, don't check any access permissions. */
    mmu_set_domain_access(0, MMU_DOMAIN_ACCESS_MANAGER);

    /* Explicitly flush the TLB before flipping the big red switch... */
    mmu_flush_tlb();

    /* Turn it on! */
    mmu_enable();

    /* And enable the caches, too. */
    cache_enable();

    return (uint8_t *)(baseaddr + PAGE_TABLE_SIZE);
}
