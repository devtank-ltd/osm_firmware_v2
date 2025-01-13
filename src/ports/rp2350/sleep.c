#include <stdio.h>
#include <stdlib.h>

#include "pico/time.h"

#include <osm/core/sleep.h>
#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/core/uart_rings.h>
#include <osm/core/measurements.h>
#include <osm/core/platform.h>
#include "pinmap.h"


#define SLEEP_LSI_CLK_FREQ_KHZ          32


static volatile uint16_t _sleep_compare = 0;


void _sleep_before_sleep(void)
{
    platform_watchdog_init(IWDG_MAX_TIME_MS);
}


void _sleep_on_wakeup(void)
{
    platform_watchdog_init(IWDG_NORMAL_TIME_MS);
}


void _sleep_enter_sleep_mode(void)
{
}


void sleep_exit_sleep_mode(void)
{
}


bool sleep_for_ms(uint32_t ms)
{
    if (ms > SLEEP_MAX_TIME_MS)
        ms = SLEEP_MAX_TIME_MS;
    else if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    uint32_t before_time = get_since_boot_ms();
    sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (uart_rings_out_busy())
    {
        uart_rings_out_drain();
        uart_ring_in_drain(COMMS_UART);
    }
    uint32_t time_passed = since_boot_delta(get_since_boot_ms(), before_time);
    if (ms <= time_passed)
        return false;
    ms -= time_passed;
    if (ms < SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    _sleep_before_sleep();
    _sleep_enter_sleep_mode();
    sleep_ms(ms);
    _sleep_on_wakeup();
    sleep_debug("Woken back up after %"PRIu32"ms.", since_boot_delta(get_since_boot_ms(), before_time));
    return true;
}


static command_response_t _sleep_cb(char* args, cmd_ctx_t * ctx)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        cmd_ctx_out(ctx, "<TIME(MS)>");
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx, "Sleeping for %"PRIu32"ms.", sleep_ms);
    sleep_for_ms(sleep_ms);
    return COMMAND_RESP_OK;
}


static command_response_t _sleep_power_mode_cb(char* args, cmd_ctx_t * ctx)
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
