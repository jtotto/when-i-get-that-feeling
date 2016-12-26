#include <caboose/platform.h>
#include <caboose/state.h>
#include <caboose/syscall.h>

#include "debug.h"
#include "frames.h"
#include "irq.h"
#include "pl011-uart.h"
#include "syscalltable.h"

extern uint8_t bss_start;
extern uint8_t bss_end;

void caboose_init(uint8_t *pool);
uint8_t *exception_init(uint8_t *pool);

#if YOU_NEED_THIS_LATER
static void sys_Unimplemented(void)
{
    ASSERT(false);
}
#endif

void sys_Assert(const char *msg);

/* We exploit AAPCS to simplify our syscall mechanism, leaving the argument
 * registers untouched during the system call context switch so that all of the
 * arguments are where they need to be when we blx to the kernel's
 * implementation.  However, since there are only 4 argument registers we need
 * to provide a little manual shim for Send(), which takes 5 arguments. */
static int sys_SendShim(tid_t tid, void *msg, int msglen, void *reply)
{
    /* The fifth argument is buried under the svcframe. */
    struct task *active = caboose.tasks.active;
    int replylen = *(int *)(active->sp + sizeof(struct svcframe));
    return sys_Send(tid, msg, msglen, reply, replylen);
}

void *syscalls[] = {
    [SYSCALL_CREATE] = sys_Create,
    [SYSCALL_MYTID] = sys_MyTid,
    [SYSCALL_MYPARENTTID] = sys_MyParentTid,
    [SYSCALL_PASS] = sys_Pass,
    [SYSCALL_EXIT] = sys_Exit,
    [SYSCALL_SEND] = sys_SendShim,
    [SYSCALL_RECEIVE] = sys_Receive,
    [SYSCALL_REPLY] = sys_Reply,
    [SYSCALL_ASYNC_SEND] = sys_AsyncSend,
    [SYSCALL_ASYNC_RECEIVE] = sys_AsyncReceive,
    [SYSCALL_AWAITEVENT] = sys_AwaitEvent,
    [SYSCALL_ASSERT] = sys_Assert
};

void platform_init(uint8_t *pool)
{
    /* Clear the bss section. */
    memset(&bss_start, 0, &bss_end - &bss_start);

    pool = uart0_init(pool);
    pool = irq_init(pool);

    /* Hand it over to the generic kernel initialization, which will start the
     * scheduler when it's ready. */
    caboose_init(pool);
}

#define USER_MODE 0b10000

void platform_task_init(struct task *task, task_f code)
{
    /* Initialize a svcframe at the top of the task's stack suitable for
     * resumption from within the engine. */
    task->sp -= sizeof(struct svcframe);

    struct svcframe *sf = (struct svcframe *)task->sp;
    *sf = (struct svcframe) {
        .sf_spsr = USER_MODE,
        .sf_r0 = 0xcab005e,
        .sf_r4 = 4,
        .sf_r5 = 5,
        .sf_r6 = 6,
        .sf_r7 = 7,
        .sf_r8 = 8,
        .sf_r9 = 9,
        .sf_r10 = 10,
        .sf_r11 = 11,
        .sf_lr = (uint32_t)code
    };
}

/* Leave these as stubs until we care about scheduler accounting. */
uint16_t platform_scheduling_timer_read(void)
{
    return 0;
}

void platform_scheduling_timer_reset(void)
{
    /* XXX */
}

void sys_Assert(const char *msg)
{
    debug_printf(msg);
    while(true) {
        /* do nothing */
    }
}
