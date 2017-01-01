#ifndef CABOOSE_PLATFORM_INLINE_H
#define CABOOSE_PLATFORM_INLINE_H

#include <stdint.h>

#include <mini-printf.h>

#include <caboose/state.h>
#include <caboose/util.h>

#include "frames.h"

void Assert(const char *msg) __noreturn;

#define ASSERT(pred) do {                                               \
    if (!(pred)) {                                                      \
        char buf[1024];                                                 \
        int size = mini_snprintf(buf,                                   \
                                 sizeof buf,                            \
                                 "Assertion failed %s %d",              \
                                 __FILE__,                              \
                                 __LINE__);                             \
        buf[size] = '\0';                                               \
        Assert(buf);                                                    \
    }                                                                   \
} while (0)

struct task;

static inline void platform_task_return_value(struct task *task, uint32_t rv)
{
    struct svcframe *sf = (struct svcframe *)task->sp;
    sf->sf_r0 = rv;
}

#endif
