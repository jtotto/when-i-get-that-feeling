#ifndef CABOOSE_PLATFORM_MMU_H
#define CABOOSE_PLATFORM_MMU_H

#include <stdint.h>

#define SECTION_SIZE (1 << 20)

/* Allocate space sufficient for a page table out of the pool. */
uint8_t *mmu_pagetable_alloc(uint8_t *pool, void **pagetable);

/* Given memory for a pagetable allocated with the above, initialize the MMU for
 * this core. */
void mmu_init(void *tablemem);

/* Mark the given section as strongly-ordered (uncacheable and unbufferable) in
 * the page table. */
void mmu_mark_strongly_ordered(void *tablemem, void *section);

/* Clean the _data_ cache for the given range by flushing any cache lines for
 * the range to main memory. */
void cache_clean_range(void *addr, int len);

/* Invalidate the data cache for the given range by invalidating all of the
 * cache lines for the range. */
void cache_invalidate_range(void *addr, int len);

/* Both of the above with a single instruction. */
void cache_clean_and_invalidate_range(void *addr, int len);

/* Syscall wrappers for the above... */
void Clean(void *addr, int len);
void Invalidate(void *addr, int len);
void CleanAndInvalidate(void *addr, int len);
/* ... with identically-typed internals. */
void sys_Clean(void *addr, int len);
void sys_Invalidate(void *addr, int len);
void sys_CleanAndInvalidate(void *addr, int len);

#endif
