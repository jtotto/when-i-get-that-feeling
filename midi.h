#ifndef SXLHLG_MIDI_H
#define SXLHLG_MIDI_H

#include <caboose-platform/midi.h>

#include "messages.h"

struct midireq {
    struct msghdr hdr;
    struct usbmidipkt pkt;
};

#define MIDI_SINK "midisink"

void midisrc(void);

#endif
