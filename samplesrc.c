#include <limits.h>
#include <stdint.h>

#include <caboose/caboose.h>
#include <caboose/platform.h>
#include <caboose/util.h>

#include <caboose-platform/debug.h>

#include "audio.h"

/* This is probably undefined behaviour.  Meh. */
extern int16_t sample_bin[];
extern unsigned int sample_bin_len;

uint32_t resampled[500000];

void samplesrc(void)
{
    /* Our sample is raw signed 16-bit little-endian 44100Hz, while we need to
     * produce unsigned 12-bit little-endian 44100Hz (in 32-bit words), so we
     * need to do some resampling. */
    debug_printf("Starting to resample...");
    for (int i = 0; i < sample_bin_len / 2; i++) {
        resampled[i] = (uint32_t)(sample_bin[i] + (-SHRT_MIN)) >> 4;
    }
    debug_printf("Finished resampling!");

    RegisterAs(AUDIO_SOURCE);

    size_t i = 0;
    while (true) {
        tid_t sender;
        struct audioreq req;
        int recvd = Receive(&sender, &req, sizeof req);
        ASSERT(recvd == sizeof req);
        ASSERT(req.hdr.type == GET_AUDIO);

        uint32_t out[req.len * 2];
        memcpy(out, &resampled[i], req.len * 2 * sizeof resampled[0]);
        i += req.len * 2;

        if (i + req.len * 2 > sample_bin_len / 2) {
            i = 0;
        }

        Reply(sender, out, sizeof out);
    }
}
