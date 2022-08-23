#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

#include "platform.h"
#include "common.h"
#include "log.h"
#include "uart_rings.h"

#include "cmd.h"
#include "uarts.h"
#include "adcs.h"
#include "io.h"
#include "i2c.h"
#include "persist_config.h"
#include "lorawan.h"
#include "measurements.h"
#include "debug_mode.h"

#include "pulsecount.h"
#include "sai.h"
#include "hpm.h"
#include "modbus.h"
#include "timers.h"
#include "htu21d.h"
#include "veml7700.h"
#include "cc.h"
#include "can_impl.h"
#include "ds18b20.h"


#define SLOW_FLASHING_TIME_SEC              3000
#define NORMAL_FLASHING_TIME_SEC            1000


int main(void)
{
    platform_init();
    platform_blink_led_init();

    i2c_init(0);

    uarts_setup();
    uart_rings_init();

    platform_raw_msg("----start----");
    log_sys_debug("Frequency : %"PRIu32, rcc_ahb_frequency);
    log_sys_debug("Version : %s", GIT_VERSION);

    persistent_init();
    timers_init();
    log_init();
    cmds_init();
    ios_init();
    sai_init();
    adcs_init();
    cc_init();
    htu21d_init();
    veml7700_init();
    ds18b20_temp_init();
    sai_init();
    pulsecount_init();
    modbus_init();
    can_impl_init();
    lw_init();

    platform_watchdog_init(IWDG_NORMAL_TIME_MS);

    log_async_log = true;

    measurements_init();

    bool boot_debug_mode = DEBUG_MODE & persist_get_log_debug_mask();

    if (boot_debug_mode)
    {
        log_sys_debug("Booted in debug_mode");
        hpm_enable(true);
        debug_mode();
        hpm_enable(false);
        log_sys_debug("Left debug_mode");
    }

    uint32_t prev_now = 0;
    uint32_t flashing_delay = SLOW_FLASHING_TIME_SEC;
    while(platform_running())
    {
        platform_watchdog_reset();
        while(since_boot_delta(get_since_boot_ms(), prev_now) < flashing_delay)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            measurements_loop_iteration();
        }
        lw_loop_iteration();
        flashing_delay = lw_get_connected()?NORMAL_FLASHING_TIME_SEC:SLOW_FLASHING_TIME_SEC;
        platform_blink_led_toggle();

        prev_now = get_since_boot_ms();
    }

    platform_deinit();

    return 0;
}
