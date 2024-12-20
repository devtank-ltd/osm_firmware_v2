#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#include "platform.h"
#include "platform_model.h"

#include "common.h"
#include "log.h"
#include "uart_rings.h"

#include "cmd.h"
#include "uarts.h"
#include "adcs.h"
#include "io.h"
#include "i2c.h"
#include "persist_config.h"
#include "protocol.h"
#include "measurements.h"


#define DISCONNECTED_FLASHING_TIME_SEC      50
#define NORMAL_FLASHING_TIME_SEC            500


int osm_main(void)
{
    platform_init();
    platform_blink_led_init();

    i2cs_init();

    uarts_setup();
    uart_rings_init();

    platform_raw_msg("----start----");
    log_sys_debug("Frequency : %"PRIu32, platform_get_frequency());
    log_sys_debug("Version : %s", GIT_VERSION);

    persistent_init();
    log_init();
    cmds_init();

    model_sensors_init();

    protocol_system_init();

    platform_watchdog_init(IWDG_NORMAL_TIME_MS);

    measurements_init();

    model_post_init();

    platform_start();
    log_async_log = true;

    uint32_t prev_now = 0;
    uint32_t flashing_delay = DISCONNECTED_FLASHING_TIME_SEC;
    while(platform_running())
    {
        platform_watchdog_reset();
        while(since_boot_delta(get_since_boot_ms(), prev_now) < flashing_delay)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            measurements_loop_iteration();
            platform_tight_loop();
            platform_main_loop_iterate();
        }
        protocol_loop_iteration();
        flashing_delay = protocol_get_connected()?NORMAL_FLASHING_TIME_SEC:DISCONNECTED_FLASHING_TIME_SEC;
        platform_blink_led_toggle();

        prev_now = get_since_boot_ms();
    }

    platform_deinit();

    return 0;
}
