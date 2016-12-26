#include <stdbool.h>

#include <caboose/caboose.h>
#include <caboose-platform/debug.h>
#include <caboose-platform/platform-events.h>
#include <caboose-platform/timer.h>

void application(void)
{
    debug_printf("Get up, get up, get up, get up!");
    while (true) {
        int data = AwaitEvent(TIMER_EVENTID);
        ASSERT(data == 0xcab005e);

        debug_printf("tick");
    }
}
