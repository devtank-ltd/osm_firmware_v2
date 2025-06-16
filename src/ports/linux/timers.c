#include <unistd.h>

#include <osm/core/timers.h>
#include "linux.h"



void osm_timer_delay_us(uint16_t wait_us)
{
    osm_linux_usleep(wait_us);
}


void osm_timer_delay_us_64(uint64_t wait_us)
{
    while (wait_us > UINT16_MAX)
    {
        osm_timer_delay_us(UINT16_MAX);
        wait_us -= UINT16_MAX;
    }
    osm_timer_delay_us(wait_us);
}


void osm_timers_init()
{
}
