#ifndef CABOOSE_PLATFORM_FRAMES_H
#define CABOOSE_PLATFORM_FRAMES_H

#include <stdint.h>

#include <caboose/util.h>

struct svcframe {
    uint32_t sf_spsr;
    uint32_t sf_r0;
    uint32_t sf_r4;
    uint32_t sf_r5;
    uint32_t sf_r6;
    uint32_t sf_r7;
    uint32_t sf_r8;
    uint32_t sf_r9;
    uint32_t sf_r10;
    uint32_t sf_r11;
    uint32_t sf_lr;
} __packed;

struct irqframe {
    uint32_t if_spsr;
    uint32_t if_lr_irq;
    uint32_t if_r0;
    uint32_t if_r1;
    uint32_t if_r2;
    uint32_t if_r3;
    uint32_t if_r4;
    uint32_t if_r5;
    uint32_t if_r6;
    uint32_t if_r7;
    uint32_t if_r8;
    uint32_t if_r9;
    uint32_t if_r10;
    uint32_t if_r11;
    uint32_t if_r12;
    uint32_t if_lr;
} __packed;

#endif
