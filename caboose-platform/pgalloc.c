#include <caboose/list.h>
#include <caboose/util.h>

#include "pgalloc.h"

static struct list freelist;

uint8_t *pgalloc_init(uint8_t *pool)
{
    /* Initialize the empty global free list. */
    list_init(&freelist);

    /* Align the pool pointer to the start of a page. */
    pool = (uint8_t *)ALIGN((uintptr_t)pool, PAGE_SIZE);

    /* Put all of our pages on the free list. */
    for (int i = 0; i < PAGE_COUNT; i++, pool += PAGE_SIZE) {
        struct node *node = (struct node *)pool;
        list_insert_tail(&freelist, node);
    }

    return pool;
}

void *pgalloc(void)
{
    return list_empty(&freelist) ? NULL : list_remove_head(&freelist);
}

void pgfree(void *page)
{
    struct node *node = page;
    list_insert_tail(&freelist, node);
}
