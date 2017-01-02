#ifndef CABOOSE_PLATFORM_MMU_H
#define CABOOSE_PLATFORM_MMU_H

#include <stdint.h>

uint8_t *mmu_init(uint8_t *pool);

/* Clean the _data_ cache for the given range by flushing any cache lines for
 * the range to main memory. */
void dcachectl_clean_range(void *addr, int len);

/* Invalidate the data cache for the given range by invalidating all of the
 * cache lines for the range. */
void dcachectl_invalidate_range(void *addr, int len);

/* Both of the above with a single instruction. */
void dcachectl_clean_and_invalidate_range(void *addr, int len);

/* Syscall wrappers for the above... */
void Clean(void *addr, int len);
void Invalidate(void *addr, int len);
void CleanAndInvalidate(void *addr, int len);
/* ... with identically-typed internals. */
void sys_Clean(void *addr, int len);
void sys_Invalidate(void *addr, int len);
void sys_CleanAndInvalidate(void *addr, int len);

#endif
