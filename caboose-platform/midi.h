#ifndef CABOOSE_PLATFORM_MIDI_H
#define CABOOSE_PLATFORM_MIDI_H

#include <stdint.h>

#include <caboose/config.h>
#include <caboose/util.h>

struct usbmidipkt {
    uint32_t len;
    uint8_t __aligned(4) packet[CONFIG_USB_PACKET_BUF_SIZE];
};

#define USBMIDIPKT_SIZE(p) (offsetof(struct usbmidipkt, packet) + (p)->len)

#endif
