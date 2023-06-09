#include <stdio.h>
#include <stdlib.h>

#include "sleep.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "measurements.h"
#include "pinmap.h"
#include "adcs.h"
#include "i2c.h"



void sleep_exit_sleep_mode(void)
{
}


bool sleep_for_ms(uint32_t ms)
{
    if (ms > SLEEP_MAX_TIME_MS)
        ms = SLEEP_MAX_TIME_MS;
    else if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    return false;
}


static command_response_t _sleep_cb(char* args)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        log_out("<TIME(MS)>");
        return COMMAND_RESP_ERR;
    }
    log_out("Sleeping for %"PRIu32"ms.", sleep_ms);
    sleep_for_ms(sleep_ms);
    return COMMAND_RESP_OK;
}


static command_response_t _sleep_power_mode_cb(char* args)
{
    measurements_power_mode_t mode;
    if (args[0] == 'A')
        mode = MEASUREMENTS_POWER_MODE_AUTO;
    else if (args[0] == 'B')
        mode = MEASUREMENTS_POWER_MODE_BATTERY;
    else if (args[0] == 'P')
        mode = MEASUREMENTS_POWER_MODE_PLUGGED;
    else
        return COMMAND_RESP_ERR;
    measurements_power_mode(mode);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* sleep_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "sleep",        "Sleep",                    _sleep_cb                      , false , NULL },
                                       { "power_mode",   "Power mode setting",       _sleep_power_mode_cb           , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
