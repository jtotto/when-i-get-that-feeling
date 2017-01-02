#ifndef CABOOSE_PLATFORM_BARRIERS_H
#define CABOOSE_PLATFORM_BARRIERS_H

#define dmb() asm volatile ("dmb" ::: "memory")
#define dsb() asm volatile ("dsb" ::: "memory")
#define isb() asm volatile ("isb" ::: "memory")

#endif
