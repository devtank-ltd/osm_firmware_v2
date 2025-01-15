#include <unistd.h>
#include <stdlib.h>

#include <osm/core/timers.h>
#include "linux.h"
#include "base_types.h"
#include "common.h"



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


static command_response_t _cmd_timer_cb(char* args, cmd_ctx_t * ctx)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    cmd_ctx_out(ctx,"Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
    return COMMAND_RESP_OK;
}


struct cmd_link_t* timers_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "timer",        "Test usecs timer",         _cmd_timer_cb                  , false , NULL},
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
