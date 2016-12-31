#ifndef CABOOSE_PLATFORM_PGALLOC_H
#define CABOOSE_PLATFORM_PGALLOC_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define PAGE_COUNT 1024

uint8_t *pgalloc_init(uint8_t *pool);

void *pgalloc(void);
void pgfree(void *page);

#endif
