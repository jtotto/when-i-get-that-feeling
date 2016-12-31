#ifndef SXLHLG_AUDIO_H
#define SXLHLG_AUDIO_H

#include "messages.h"

#define AUDIO_SOURCE "marvin"

struct audioreq {
    struct msghdr hdr;
    /* The number of stereo 12-bit 44100Hz samples we'd like to receive in
     * reply. */
    unsigned int len;
};

void audio(void);

#endif
