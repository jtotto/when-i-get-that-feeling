#ifndef CABOOSE_PLATFORM_IPI_H
#define CABOOSE_PLATFORM_IPI_H

#include <stdint.h>

#define IPI_USB 0

uint8_t *ipi_init(uint8_t *pool);

void ipi_register(uint8_t ipi, void (*handler)(void));

void ipi_service(void);

void ipi_deliver(uint8_t ipi);

#endif
