#ifndef SXLHLNG_MESSAGES_H
#define SXLHLNG_MESSAGES_H

#include <stdint.h>

/* Here we define a universal tag header for messages passed between
 * loosely-coupled system tasks. */

struct msghdr {
    enum {
        GET_AUDIO
    } type;
    uint8_t data[];
};

#endif
