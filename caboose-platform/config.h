#ifndef SXLHLNG_CABOOSE_CONFIG_H
#define SXLHLNG_CABOOSE_CONFIG_H

/* How many types of 'external events' does your platform/application support?
 * A common configuration maps interesting interrupts 1:1 to events. */
#define CONFIG_EVENT_COUNT 2

/* How many notifications should be buffered for each type of external event? */
#define CONFIG_EVENT_RING_COUNT 32

/* How many tasks can be created, at maximum?  All task resources are allocated
 * statically at initialization. */
#define CONFIG_TASK_COUNT 64

/* How much memory should be allocated for each task's stack? */
#define CONFIG_TASK_STACK_SIZE (64 * (1 << 10))

/* How much space should be allocated for each async message? */
#define CONFIG_ASYNC_MSG_BUFSIZE 256

/* How many async message buffers should be allocated?  We panic if we run out
 * of these. */
#define CONFIG_ASYNC_MSG_COUNT 1024

/* At what priority should the system init task run? */
#define CONFIG_INIT_PRIORITY 7

/* At what priority should the nameserver run? */
#define CONFIG_NAMESERVER_PRIORITY 2

/* At what priority should the application init task run? */
#define CONFIG_APPLICATION_INIT_PRIORITY 8

/* How many priorities should the scheduler support? */
#define CONFIG_PRIORITY_COUNT 32

#endif
