#include <stdbool.h>

#include <caboose/caboose.h>
#include <caboose/platform.h>

#include <caboose-platform/debug.h>
#include <caboose-platform/midi.h>
#include <caboose-platform/platform-events.h>

#include "messages.h"
#include "midi.h"

void midisrc(void)
{
    struct midireq req;
    tid_t midi_sink = WhoIs(MIDI_SINK);
    while (true) {
        int ev = AwaitEvent(MIDIPKT_EVENTID);
        struct usbmidipkt *pkt = (struct usbmidipkt *)ev;

        req.hdr.type = DELIVER_MIDI;
        memcpy(&req.pkt, pkt, USBMIDIPKT_SIZE(pkt));

        AcknowledgeEvent(MIDIPKT_EVENTID, ev);

        int rc = Send(midi_sink, &req, sizeof req, NULL, 0);
        ASSERT(rc == 0);
    }
}
