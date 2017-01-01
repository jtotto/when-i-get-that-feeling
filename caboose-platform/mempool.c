#include <caboose/list.h>

#include "mempool.h"

void mempool_init(struct mempool *mp, uint8_t *start, size_t size, size_t count)
{
    list_init(&mp->freelist);

    for (int i = 0; i < count; i++, start += size) {
        struct node *node = (struct node *)start;
        list_insert_tail(&mp->freelist, node);
    }
}

void *mempool_alloc(struct mempool *mp)
{
    return list_empty(&mp->freelist) ? NULL : list_remove_head(&mp->freelist);
}

void mempool_free(struct mempool *mp, void *mem)
{
    struct node *node = mem;
    list_insert_tail(&mp->freelist, node);
}
