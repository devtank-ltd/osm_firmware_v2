#include <stdio.h>
#include <stdlib.h>

#include <osm/core/sleep.h>
#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/core/uart_rings.h>
#include <osm/core/measurements.h>
#include "pinmap.h"
#include <osm/core/adcs.h>
#include <osm/core/i2c.h>



void osm_sleep_exit_sleep_mode(void)
{
}


bool osm_sleep_for_ms(uint32_t ms)
{
    if (ms > OSM_SLEEP_MAX_TIME_MS)
        ms = OSM_SLEEP_MAX_TIME_MS;
    else if (ms < OSM_SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    return false;
}


static osm_command_response_t _sleep_cb(char* args, osm_cmd_ctx_t * ctx)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        osm_cmd_ctx_out(ctx, "<TIME(MS)>");
        return OSM_COMMAND_RESP_ERR;
    }
    osm_cmd_ctx_out(ctx, "Sleeping for %"PRIu32"ms.", sleep_ms);
    osm_sleep_for_ms(sleep_ms);
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _sleep_power_mode_cb(char* args, osm_cmd_ctx_t * ctx)
{
    osm_measurements_power_mode_t mode;
    if (args[0] == 'A')
        mode = OSM_MEASUREMENTS_POWER_MODE_AUTO;
    else if (args[0] == 'B')
        mode = OSM_MEASUREMENTS_POWER_MODE_BATTERY;
    else if (args[0] == 'P')
        mode = OSM_MEASUREMENTS_POWER_MODE_PLUGGED;
    else
        return OSM_COMMAND_RESP_ERR;
    osm_measurements_power_mode(mode);
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* osm_sleep_add_commands(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] = {{ "sleep",        "Sleep",                    _sleep_cb                      , false , NULL },
                                       { "power_mode",   "Power mode setting",       _sleep_power_mode_cb           , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
