#include <unistd.h>
#include <stdlib.h>

#include <osm/core/base_types.h>
#include <osm/core/common.h>
#include <osm/core/timers.h>
#include <osm/ports/linux/linux.h>



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


static osm_command_response_t _cmd_timer_cb(char* args, osm_cmd_ctx_t * ctx)
{
    char* pos = osm_skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = osm_get_since_boot_ms();
    osm_timer_delay_us_64(delay_ms * 1000);
    osm_cmd_ctx_out(ctx,"Time elapsed: %"PRIu32, osm_since_boot_delta(osm_get_since_boot_ms(), start_time));
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* timers_add_commands(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] =
    {
        { "timer",        "Test usecs timer",         _cmd_timer_cb                  , false , NULL},
    };
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
