#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#include <osm/core/platform.h>
#include "platform_model.h"

#include <osm/core/common.h>
#include <osm/core/log.h>
#include <osm/core/uart_rings.h>

#include <osm/core/cmd.h>
#include <osm/core/uarts.h>
#include <osm/core/adcs.h>
#include <osm/core/io.h>
#include <osm/core/i2c.h>
#include <osm/core/persist_config.h>
#include <osm/protocols/protocol.h>
#include <osm/core/measurements.h>


#define DISCONNECTED_FLASHING_TIME_SEC      50
#define NORMAL_FLASHING_TIME_SEC            500


int osm_main(void)
{
    osm_platform_init();
    osm_platform_blink_led_init();

    osm_i2cs_init();

    osm_uarts_setup();
    osm_uart_rings_init();

    osm_platform_raw_msg("----start----");
    osm_log_sys_debug("Frequency : %"PRIu32, osm_platform_get_frequency());
    osm_log_sys_debug("Version : %s", GIT_VERSION);

    osm_persistent_init();
    osm_log_init();
    osm_cmds_init();

    osm_model_sensors_init();

    osm_protocol_system_init();

    osm_platform_watchdog_init(OSM_IWDG_NORMAL_TIME_MS);

    osm_measurements_init();

    osm_model_post_init();

    osm_platform_start();
    log_async_log = true;

    uint32_t prev_now = 0;
    uint32_t flashing_delay = DISCONNECTED_FLASHING_TIME_SEC;
    while(osm_platform_running())
    {
        osm_platform_watchdog_reset();
        while(osm_since_boot_delta(osm_get_since_boot_ms(), prev_now) < flashing_delay)
        {
            osm_uart_rings_in_drain();
            osm_uart_rings_out_drain();
            osm_measurements_loop_iteration();
            osm_platform_tight_loop();
            osm_platform_main_loop_iterate();
        }
        osm_protocol_loop_iteration();
        flashing_delay = osm_protocol_get_connected()?NORMAL_FLASHING_TIME_SEC:DISCONNECTED_FLASHING_TIME_SEC;
        osm_platform_blink_led_toggle();

        prev_now = osm_get_since_boot_ms();
    }

    osm_platform_deinit();

    return 0;
}
