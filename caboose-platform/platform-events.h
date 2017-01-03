#ifndef CABOOSE_PLATFORM_EVENTS_H
#define CABOOSE_PLATFORM_EVENTS_H

#define TIMER_EVENTID 0
#define DMA0_EVENTID 1
#define MIDIPKT_EVENTID 2

/* We extend the standard CaboOSe kernel API here with an additional primitive,
 * which permits userspace to 'complete' receipt of a previously awaited event
 * and return resources to the platform. */
int AcknowledgeEvent(int eventid, int ack);

/* The corresponding platform-internal implementation. */
int sys_AcknowledgeEvent(int eventid, int ack);

void event_register_ack_handler(int eventid, void (*handler)(int ack));

#endif
