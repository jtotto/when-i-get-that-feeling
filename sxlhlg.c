#include <stdbool.h>

#include <caboose/caboose.h>
#include <caboose-platform/debug.h>
#include <caboose-platform/platform-events.h>

#include "audio.h"

void samplesrc(void);

void application(void)
{
    debug_printf("Get up, get up, get up, get up!");

    while (true) {
        /* Eat the timer events. */
        AwaitEvent(TIMER_EVENTID);
        debug_printf("tick");
    }
}
