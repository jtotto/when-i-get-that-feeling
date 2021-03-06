#include <caboose/platform.h>
#include <caboose/state.h>
#include <caboose/syscall.h>

#include "debug.h"
#include "frames.h"
#include "ipi.h"
#include "irq.h"
#include "mmu.h"
#include "pl011-uart.h"
#include "platform-events.h"
#include "secondary.h"
#include "syscalltable.h"
#include "timer.h"
#include "usb.h"

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
    [SYSCALL_ASSERT] = sys_Assert,
    [SYSCALL_CLEAN] = sys_Clean,
    [SYSCALL_INVALIDATE] = sys_Invalidate,
    [SYSCALL_CLEAN_AND_INVALIDATE] = sys_CleanAndInvalidate,
    [SYSCALL_ACKNOWLEDGEEVENT] = sys_AcknowledgeEvent
};

void platform_init(uint8_t *pool)
{
    /* Clear the bss section. */
    memset(&bss_start, 0, &bss_end - &bss_start);

    /* Bring the UART up first so that debug logging is available to the rest of
     * the initialization routines. */
    pool = uart0_init(pool);

    void *pagetable;
    pool = mmu_pagetable_alloc(pool, &pagetable);
    mmu_init(pagetable);

    pool = irq_init(pool);
    pool = ipi_init(pool);
    pool = timer_init(pool);
    pool = usb_init(pool);

    /* Put the unused cores to sleep.  Would be good if there was some way I
     * could automatically do this, but for now this just has to be kept in sync
     * with the behaviour of the rest of the platform code manually. */
    secondary_start(2, NULL);
    secondary_start(3, NULL);

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

static void (*ack_handlers[CONFIG_EVENT_COUNT])(int ack);

void event_register_ack_handler(int eventid, void (*handler)(int ack))
{
    ASSERT(!ack_handlers[eventid]);
    ack_handlers[eventid] = handler;
}

int sys_AcknowledgeEvent(int eventid, int ack)
{
    ASSERT(eventid < CONFIG_EVENT_COUNT);
    ASSERT(ack_handlers[eventid]);
    ack_handlers[eventid](ack);
    return 0;
}
