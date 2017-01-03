#include <stdbool.h>

#include <caboose/caboose.h>

#include <caboose-platform/debug.h>
#include <caboose-platform/midi.h>
#include <caboose-platform/platform-events.h>

void midisrc(void)
{
    while (true) {
        int ev = AwaitEvent(MIDIPKT_EVENTID);
        struct usbmidipkt *pkt = (struct usbmidipkt *)ev;
        debug_printf("midisrc received packet with length %u!", pkt->len);
        AcknowledgeEvent(MIDIPKT_EVENTID, ev);
    }
}
