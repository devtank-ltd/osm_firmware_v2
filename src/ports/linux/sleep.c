#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <osm/core/sleep.h>

#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/core/uart_rings.h>
#include <osm/core/measurements.h>
#include <osm/core/adcs.h>
#include <osm/core/platform.h>
#include "linux.h"
#include "pinmap.h"


#define SLEEP_LSI_CLK_FREQ_KHZ          32


static volatile uint16_t _sleep_compare = 0;


void _sleep_before_sleep(void)
{
    osm_adcs_off();
    osm_platform_watchdog_init(OSM_IWDG_MAX_TIME_MS);
}


void _sleep_on_wakeup(void)
{
    osm_adcs_init();
    osm_platform_watchdog_init(OSM_IWDG_NORMAL_TIME_MS);
}


void osm_sleep_exit_sleep_mode(void)
{
    osm_linux_awaken();
}


bool osm_sleep_for_ms(uint32_t ms)
{
    if (ms > OSM_SLEEP_MAX_TIME_MS)
        ms = OSM_SLEEP_MAX_TIME_MS;
    uint32_t    before_time = osm_get_since_boot_ms();
    osm_sleep_debug("Sleeping for %"PRIu32"ms.", ms);
    while (osm_uart_rings_out_busy())
    {
        osm_uart_rings_out_drain();
        osm_uart_ring_in_drain(COMMS_UART);
    }
    uint32_t    time_passed = osm_since_boot_delta(osm_get_since_boot_ms(), before_time);
    if (ms <= time_passed)
        return false;
    ms -= time_passed;
    if (ms < OSM_SLEEP_MIN_SLEEP_TIME_MS)
        return false;
    /* Must sort out count and prescale depending on size of count */
    uint32_t    div_shift   = 0;
    uint64_t    count       = ms;
    while (count >= (SLEEP_LSI_CLK_FREQ_KHZ * UINT16_MAX / 1000))
    {
        count /= 2;
        div_shift += 1;
        if (div_shift > 7)
        {
            // Reduced the maximum value of sleep by 1 count so theres enough time for watchdog.
            div_shift = 7;
            goto turn_on_sleep;
        }
    }
    count = (ms * 1000) / (SLEEP_LSI_CLK_FREQ_KHZ * (1 << div_shift));
turn_on_sleep:
    _sleep_before_sleep();
    osm_linux_usleep(ms * 1000);
    _sleep_on_wakeup();
    osm_sleep_debug("Woken back up after %"PRIu32"ms.", osm_since_boot_delta(osm_get_since_boot_ms(), before_time));
    return true;
}


static command_response_t _sleep_cb(char* args, cmd_ctx_t * ctx)
{
    char* p;
    uint32_t sleep_ms = strtoul(args, &p, 10);
    if (p == args)
    {
        osm_cmd_ctx_out(ctx,"<TIME(MS)>");
        return COMMAND_RESP_ERR;
    }
    osm_cmd_ctx_out(ctx,"Sleeping for %"PRIu32"ms.", sleep_ms);
    osm_sleep_for_ms(sleep_ms);
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
    osm_measurements_power_mode(mode);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* osm_sleep_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "sleep",        "Sleep",                    _sleep_cb                      , false , NULL },
                                       { "power_mode",   "Power mode setting",       _sleep_power_mode_cb           , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}
