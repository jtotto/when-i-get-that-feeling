#ifndef SXLHLG_MESSAGES_H
#define SXLHLG_MESSAGES_H

#include <stdint.h>

/* Here we define a universal tag header for messages passed between
 * loosely-coupled system tasks. */

struct msghdr {
    enum {
        GET_AUDIO,
        DELIVER_MIDI
    } type;
    uint8_t data[];
};

#endif
