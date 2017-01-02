#ifndef CABOOSE_PLATFORM_MMU_H
#define CABOOSE_PLATFORM_MMU_H

#include <stdint.h>

uint8_t *mmu_init(uint8_t *pool);

/* Clean the _data_ cache for the given range by flushing any cache lines for
 * the range to main memory. */
void dcachectl_clean_range(void *addr, uint32_t len);

/* Invalidate the data cahce for the given range by invalidating all of the
 * cache lines for the range. */
void dcachectl_invalidate_range(void *addr, uint32_t len);

#endif
