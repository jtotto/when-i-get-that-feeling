#include <stdbool.h>

#include <caboose/caboose.h>
#include <caboose-platform/debug.h>
#include <caboose-platform/platform-events.h>

#include "audio.h"
#include "midi.h"
#include "synth.h"

void application(void)
{
    debug_printf("Get up, get up, get up, get up!");

    Create(1, audio);
    Create(2, synth);
    Create(5, midisrc);

    Exit();
}
