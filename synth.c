#include <stdbool.h>

#include <caboose/caboose.h>
#include <caboose/platform.h>
#include <caboose/util.h>

#include "audio.h"
#include "messages.h"
#include "midi.h"
#include "synth.h"

static int periods[] = {
	5394,
	5091,
	4805,
	4536,
	4281,
	4041,
	3814,
	3600,
	3398,
	3207,
	3027,
	2857,
	2697,
	2546,
	2403,
	2268,
	2141,
	2020,
	1907,
	1800,
	1699,
	1604,
	1514,
	1429,
	1348,
	1273,
	1201,
	1134,
	1070,
	1010,
	954,
	900,
	849,
	802,
	757,
	714,
	674,
	636,
	601,
	567,
	535,
	505,
	477,
	450,
	425,
	401,
	378,
	357,
	337,
	318,
	300,
	283,
	268,
	253,
	238,
	225,
	212,
	200,
	189,
	179,
	169,
	159,
	150,
	142,
	134,
	126,
	119,
	113,
	106,
	100,
	95,
	89,
	84,
	80,
	75,
	71,
	67,
	63,
	60,
	56,
	53,
	50,
	47,
	45,
	42,
	40,
	38,
	35,
	33,
	32,
	30,
	28,
	27,
	25,
	24,
	22,
	21,
	20,
	19,
	18,
	17,
	16,
	15,
	14,
	13,
	13,
	12,
	11,
	11,
	10,
	9,
	9,
	8,
	8,
	7,
	7,
	7,
	6,
	6,
	6,
	5,
	5,
	5,
	4,
	4,
	4,
	4,
	4
};

#define MIDI_NOTE_OFF   0b1000
#define MIDI_NOTE_ON    0b1001

#define SAMPLE_HIGH 6144
#define SAMPLE_MID 4096
#define SAMPLE_LOW 2048

static int fill(uint32_t *buf, int count, int period, int offset)
{
    int midpoint = period / 2;
    for (int i = 0; i < count; i++) {
        uint32_t sample = offset < midpoint ? SAMPLE_HIGH : SAMPLE_LOW;
        *buf++ = sample;
        *buf++ = sample;
        offset = (offset + 1) % period;
    }

    return offset;
}

void synth(void)
{
    RegisterAs(AUDIO_SOURCE);
    RegisterAs(MIDI_SINK);

    union {
        struct msghdr hdr;
        struct audioreq a;
        struct midireq m;
    } req;

    int note = -1;
    int period_offset = 0;

    while (true) {
        tid_t sender;
        Receive(&sender, &req, sizeof req);

        switch (req.hdr.type) {
        case DELIVER_MIDI:
        {
            /* No sense delaying the MIDI task here. */
            Reply(sender, NULL, 0);

            uint8_t status = req.m.pkt.packet[1];
            uint8_t type = status >> 4;
            switch (type) {
            case MIDI_NOTE_OFF:
            {
                int off_note = req.m.pkt.packet[2];
                if (note == off_note) {
                    note = -1;
                }
                break;
            }
            case MIDI_NOTE_ON:
                note = req.m.pkt.packet[2];
                period_offset = 0;
                break;
            }
            break;
        }
        case GET_AUDIO:
        {
            uint32_t out[req.a.len * 2];
            if (note < 0) {
                /* Fill the output buffer with silence. */
                for (int i = 0; i < req.a.len * 2; i++) {
                    out[i] = SAMPLE_MID;
                }
            } else {
                /* Fill the output buffer with audio at the frequency of the
                 * note last played. */
                int period = periods[note];
                period_offset = fill(out, req.a.len, period, period_offset);
            }

            Reply(sender, out, sizeof out);
            break;
        }
        default:
            ASSERT(false);
        }
    }
}
