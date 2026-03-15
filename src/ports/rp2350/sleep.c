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
    osm_platform_watchdog_init(OSM_IWDG_MAX_TIME_MS);
}


void _sleep_on_wakeup(void)
{
    osm_platform_watchdog_init(OSM_IWDG_NORMAL_TIME_MS);
}


void _sleep_enter_sleep_mode(void)
{
}


void osm_sleep_exit_sleep_mode(void)
{
}


bool osm_sleep_for_ms(uint32_t ms)
{
    if (ms > OSM_SLEEP_MAX_TIME_MS)
        ms = OSM_SLEEP_MAX_TIME_MS;
    else if (ms < OSM_SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    uint32_t before_time = osm_get_since_boot_ms();
    osm_sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (osm_uart_rings_out_busy())
    {
        osm_uart_rings_out_drain();
        osm_uart_ring_in_drain(COMMS_UART);
    }
    uint32_t time_passed = osm_since_boot_delta(osm_get_since_boot_ms(), before_time);
    if (ms <= time_passed)
        return false;
    ms -= time_passed;
    if (ms < OSM_SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    _sleep_before_sleep();
    _sleep_enter_sleep_mode();
    sleep_ms(ms);
    _sleep_on_wakeup();
    osm_sleep_debug("Woken back up after %"PRIu32"ms.", osm_since_boot_delta(osm_get_since_boot_ms(), before_time));
    return true;
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
