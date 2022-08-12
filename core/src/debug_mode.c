#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/iwdg.h>

#include "measurements.h"
#include "persist_config.h"
#include "base_types.h"
#include "value.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "lorawan.h"

#include "can_impl.h"
#include "ds18b20.h"
#include "hpm.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "sai.h"
#include "veml7700.h"
#include "cc.h"

#include "modbus_measurements.h"


#define DEBUG_MODE_STR_LEN   10

static bool _debug_mode_enabled = false;

static unsigned _debug_mode_modbus_waiting = 0;

static bool _debug_mode_init_get_toggle = true;


static bool _debug_modbus_init(modbus_reg_t * reg, void * userdata)
{
    (void)userdata;
    if (modbus_start_read(reg))
        _debug_mode_modbus_waiting++;
    return true;
}


static void _debug_mode_init_iteration(void)
{
    htu21d_temp_measurements_init(MEASUREMENTS_HTU21D_TEMP);
    htu21d_humi_measurements_init(MEASUREMENTS_HTU21D_HUMI);
    hpm_init(MEASUREMENTS_PM25_NAME);
    hpm_init(MEASUREMENTS_PM10_NAME);
    ds18b20_measurements_init(MEASUREMENTS_W1_PROBE_NAME_1);
    sai_measurements_init(MEASUREMENTS_SOUND_NAME);
    veml7700_light_measurements_init(MEASUREMENTS_LIGHT_NAME);
    pulsecount_begin(MEASUREMENTS_PULSE_COUNT_NAME_1);
    pulsecount_begin(MEASUREMENTS_PULSE_COUNT_NAME_2);
    cc_begin(MEASUREMENTS_CURRENT_CLAMP_1_NAME);
    cc_begin(MEASUREMENTS_CURRENT_CLAMP_2_NAME);
    cc_begin(MEASUREMENTS_CURRENT_CLAMP_3_NAME);
    if (!_debug_mode_modbus_waiting)
        modbus_for_all_regs(_debug_modbus_init, NULL);
}


static void _debug_mode_send_value(measurements_sensor_state_t state, char* name, value_t * value)
{
    if (state != MEASUREMENTS_SENSOR_STATE_SUCCESS)
        goto bad_exit;

    char char_arr[DEBUG_MODE_STR_LEN] = "";
    if (value_to_str(value, char_arr, DEBUG_MODE_STR_LEN))
        dm_debug("%s:%s", name, char_arr);
        return;
bad_exit:
    dm_debug("%s:FAILED", name);
}


static void _debug_mode_collect_sensor(char* name, measurements_sensor_state_t (* function)(char* name, value_t* value))
{
    value_t value;
    _debug_mode_send_value(function(name, &value), name, &value);
}


static bool _debug_modbus_get(modbus_reg_t * reg, void * userdata)
{
    (void)userdata;
    value_t value;
    char name[MEASURE_NAME_NULLED_LEN] = {0};
    if (modbus_reg_get_name(reg, name))
    {
        measurements_sensor_state_t r = modbus_measurements_get2(reg, &value);
        if (r == MEASUREMENTS_SENSOR_STATE_SUCCESS)
        {
            _debug_mode_send_value(r, name, &value);
            _debug_mode_modbus_waiting--;
            if (!_debug_modbus_init(reg, userdata)) /*Start this one again,  marking it as not having a valid value*/
                log_error("Failed to re-queue modbus reg %s", name);
        }
    }
    return true;
}


static void _lw_send_msg(void)
{
    value_t val;
    val.type = VALUE_UINT8;
    val.u8 = 1;
    _debug_mode_send_value(measurements_send_test()?MEASUREMENTS_SENSOR_STATE_SUCCESS:MEASUREMENTS_SENSOR_STATE_ERROR, "LORA", &val);
}


static void _debug_mode_collect_iteration(void)
{
    _debug_mode_collect_sensor(MEASUREMENTS_HTU21D_TEMP, htu21d_temp_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_HTU21D_HUMI, htu21d_humi_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PM25_NAME, hpm_get_pm25);
    _debug_mode_collect_sensor(MEASUREMENTS_PM10_NAME, hpm_get_pm10);
    _debug_mode_collect_sensor(MEASUREMENTS_W1_PROBE_NAME_1, ds18b20_measurements_collect);
    _debug_mode_collect_sensor(MEASUREMENTS_SOUND_NAME, sai_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_LIGHT_NAME, veml7700_light_measurements_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PULSE_COUNT_NAME_1, pulsecount_get);
    _debug_mode_collect_sensor(MEASUREMENTS_PULSE_COUNT_NAME_2, pulsecount_get);
    _debug_mode_collect_sensor(MEASUREMENTS_CURRENT_CLAMP_1_NAME, cc_get);
    _debug_mode_collect_sensor(MEASUREMENTS_CURRENT_CLAMP_2_NAME, cc_get);
    _debug_mode_collect_sensor(MEASUREMENTS_CURRENT_CLAMP_3_NAME, cc_get);
    can_impl_send_example();
    static int lw_counter = 0;
    if (lw_counter == 10)
    {
        _lw_send_msg();
        lw_counter = 0;
        if (_debug_mode_modbus_waiting)
            modbus_for_all_regs(_debug_modbus_get, NULL);
    }
    lw_counter++;
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
    if (_debug_mode_init_get_toggle)
    {
        _debug_mode_init_iteration();
    }
    else
    {
        bool now_connected = lw_get_connected();
        if (!now_connected)
        {
            dm_debug("LORA:FAILED");
        }
        else
        {
            dm_debug("LORA:CONNECTED");
        }
        _debug_mode_collect_iteration();
    }
    _debug_mode_init_get_toggle = !_debug_mode_init_get_toggle;
}


static void _debug_mode_init(void)
{
    can_impl_init();
}


void debug_mode(void)
{
    if (_debug_mode_enabled)
    {
        /* This has been called by uart_rings_in_drain below.
         * So rather than enter the loop again, just use this as toggle.
         */
        _debug_mode_enabled = false;
        return;
    }
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    cc_set_channels_active(all_cc_channels, ADC_CC_COUNT);

    _debug_mode_enabled = true;

    log_out(LOG_START_SPACER"DEBUG_MODE"LOG_END_SPACER);

    _debug_mode_init();

    uint32_t prev_now = 0;
    while(_debug_mode_enabled || !_debug_mode_init_get_toggle) /* Do extra loop if waiting to collect sensors so not to confuse when rejoining measurements.*/
    {
        while(since_boot_delta(get_since_boot_ms(), prev_now) < 1000)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            _debug_mode_fast_iteration();
        }
        lw_loop_iteration();
        prev_now = get_since_boot_ms();
        _debug_mode_iteration();
        gpio_toggle(LED_PORT, LED_PIN);
        iwdg_reset();
    }
    measurements_derive_cc_phase();
}
