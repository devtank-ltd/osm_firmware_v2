
#include <libopencm3/cm3/scb.h>

#include "measurements.h"
#include "persist_config.h"
#include "base_types.h"
#include "value.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "lorawan.h"

#include "ds18b20.h"
#include "hpm.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "sai.h"
#include "veml7700.h"


#define DEBUG_MODE_STR_LEN   10


static bool* _debug_mode_enabled;


static void _debug_mode_init_iteration(void)
{
    htu21d_temp_measurements_init(MEASUREMENTS_HTU21D_TEMP);
    htu21d_humi_measurements_init(MEASUREMENTS_HTU21D_HUMI);
    hpm_init(MEASUREMENTS_PM25_NAME);
    hpm_init(MEASUREMENTS_PM10_NAME);
    ds18b20_measurements_init(MEASUREMENTS_W1_PROBE_NAME_1);
    ds18b20_measurements_init(MEASUREMENTS_W1_PROBE_NAME_2);
    sai_measurements_init(MEASUREMENTS_SOUND_NAME);
    veml7700_light_measurements_init(MEASUREMENTS_LIGHT_NAME);
    pulsecount_begin(MEASUREMENTS_PULSE_COUNT_NAME_1);
    pulsecount_begin(MEASUREMENTS_PULSE_COUNT_NAME_2);
}


static void _debug_mode_collect_sensor(char* name, measurements_sensor_state_t (* function)(char* name, value_t* value))
{
    value_t value;
    measurements_sensor_state_t r = function(name, &value);
    if (r != MEASUREMENTS_SENSOR_STATE_SUCCESS)
        goto bad_exit;
    char char_arr[DEBUG_MODE_STR_LEN] = "";
    if (value_to_str(&value, char_arr, DEBUG_MODE_STR_LEN))
        dm_debug("%s\t: %s", name, char_arr);
        return;
bad_exit:
    dm_debug("%s\t: FAILED", name);
}


static void _debug_mode_collect_iteration(void)
{
    _debug_mode_collect_sensor(MEASUREMENTS_HTU21D_TEMP, htu21d_temp_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_HTU21D_HUMI, htu21d_humi_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PM25_NAME, hpm_get_pm25);
    _debug_mode_collect_sensor(MEASUREMENTS_PM10_NAME, hpm_get_pm10);
    _debug_mode_collect_sensor(MEASUREMENTS_W1_PROBE_NAME_1, ds18b20_measurements_collect);
    _debug_mode_collect_sensor(MEASUREMENTS_W1_PROBE_NAME_2, ds18b20_measurements_collect);
    _debug_mode_collect_sensor(MEASUREMENTS_SOUND_NAME, sai_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_LIGHT_NAME, veml7700_light_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PULSE_COUNT_NAME_1, pulsecount_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PULSE_COUNT_NAME_2, pulsecount_get);
}


static void _debug_mode_init(void)
{
    _debug_mode_enabled = persist_get_debug_mode();
}


static void _debug_mode_fast_iteration(void)
{
    htu21d_measurements_iteration(MEASUREMENTS_HTU21D_TEMP);
    htu21d_measurements_iteration(MEASUREMENTS_HTU21D_HUMI);
    sai_iteration_callback(MEASUREMENTS_SOUND_NAME);
    veml7700_iteration(MEASUREMENTS_LIGHT_NAME);
}


static void _debug_mode_iteration(void)
{
    static bool _debug_mode_toggle = false;
    _debug_mode_toggle = !_debug_mode_toggle;
    if (_debug_mode_toggle)
    {
        _debug_mode_init_iteration();
    }
    else
    {
        _debug_mode_collect_iteration();
    }
}


void debug_mode(void)
{
    _debug_mode_init();

    log_out(LOG_START_SPACER"DEBUG_MODE"LOG_END_SPACER);

    bool prev_connected = false;
    uint32_t prev_now = 0;
    //uint32_t long_wait = 0;
    while(true)
    {
        bool now_connected = lw_get_connected();
        if (!now_connected && prev_connected)
        {
            dm_debug("LORA\t: FAILED");
            prev_connected = false;
            continue;
        }
        else if (!now_connected)
            continue;
        else if (!prev_connected)
        {
            dm_debug("LORA\t: CONNECTED");
            prev_connected = true;
        }
        while(since_boot_delta(get_since_boot_ms(), prev_now) < 1000)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            _debug_mode_fast_iteration();
        }
        _debug_mode_iteration();
        gpio_toggle(LED_PORT, LED_PIN);
        prev_now = get_since_boot_ms();
        //while
    }
}


void debug_mode_enable(bool enabled)
{
    if (*_debug_mode_enabled != enabled)
    {
        *_debug_mode_enabled = enabled;
        persist_commit();
        scb_reset_system();
    }
}
