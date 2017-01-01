#ifndef CABOOSE_PLATFORM_MEMPOOL_H
#define CABOOSE_PLATFORM_MEMPOOL_H

#include <stdint.h>

#include <caboose/list.h>

struct mempool {
    struct list freelist;
};

void mempool_init(struct mempool *mp, uint8_t *start, size_t size, size_t count);

void *mempool_alloc(struct mempool *mp);
void mempool_free(struct mempool *mp, void *mem);

#endif
