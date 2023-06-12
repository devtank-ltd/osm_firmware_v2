#include <stdlib.h>
#include <string.h>

#include "debug_mode.h"

#include "measurements.h"
#include "persist_config.h"
#include "base_types.h"
#include "log.h"
#include "common.h"
#include "uart_rings.h"
#include "protocol.h"
#include "platform.h"

#include "can_impl.h"
#include "ds18b20.h"
#include "hpm.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "sai.h"
#include "veml7700.h"
#include "cc.h"

#include "modbus_measurements.h"
#include "platform_model.h"


#define DEBUG_MODE_STR_LEN   10

static bool _debug_mode_enabled = false;

static bool _debug_mode_init_get_toggle = true;



static bool _debug_mode_init_iteration_cb(measurements_def_t* def, void *data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, NULL, &inf))
        return false;
    if ( inf.init_cb)
        inf.init_cb(def->name, false);
    return true;
}


static void _debug_mode_init_iteration(void)
{
    measurements_for_each(_debug_mode_init_iteration_cb, NULL);
}


static void _debug_mode_send_value(measurements_sensor_state_t state, char* name, measurements_value_type_t value_type, measurements_reading_t* value)
{
    if (state != MEASUREMENTS_SENSOR_STATE_SUCCESS)
        goto bad_exit;

    switch(value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            dm_debug("%s:i64:%"PRIi64, name, value->v_i64);
            break;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            dm_debug("%s:f32:%"PRIi32".%03"PRIi32, name, value->v_f32/1000, value->v_f32%1000);
            break;
        case MEASUREMENTS_VALUE_TYPE_STR:
            dm_debug("%s:str:%s", name, value->v_str);
        default:
            goto bad_exit;
    }
bad_exit:
    dm_debug("%s:FAILED", name);
}


static void _lw_send_msg(void)
{
    measurements_reading_t val;
    val.v_i64 = 1;
    _debug_mode_send_value(measurements_send_test(MEASUREMENTS_FW_VERSION)?MEASUREMENTS_SENSOR_STATE_SUCCESS:MEASUREMENTS_SENSOR_STATE_ERROR, "LORA", MEASUREMENTS_VALUE_TYPE_I64, &val);
}


static bool _debug_mode_collect_iteration_cb(measurements_def_t* def, void * data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, NULL, &inf))
        return false;
    measurements_reading_t value;
    _debug_mode_send_value(inf.get_cb(def->name, &value), def->name, inf.value_type_cb(def->name), &value);
    return true;
}


static void _debug_mode_collect_iteration(void)
{
    measurements_for_each(_debug_mode_collect_iteration_cb, NULL);
    can_impl_send_example();
    static int lw_counter = 0;
    if (lw_counter == 10)
    {
        _lw_send_msg();
        lw_counter = 0;
    }
    lw_counter++;
}


static bool _debug_mode_fast_iteration_cb(measurements_def_t* def, void * data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, NULL, &inf))
        return false;
    if (inf.iteration_cb)
        inf.iteration_cb(def->name);
    return true;
}


static void _debug_mode_fast_iteration(void)
{
    measurements_for_each(_debug_mode_fast_iteration_cb, NULL);
}


static void _debug_mode_iteration(void)
{
    if (_debug_mode_init_get_toggle)
    {
        _debug_mode_init_iteration();
    }
    else
    {
        bool now_connected = protocol_get_connected();
        if (!now_connected)
        {
            dm_debug("COMMS:FAILED");
        }
        else
        {
            dm_debug("COMMS:CONNECTED");
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
    model_debug_mode_enable_all();

    _debug_mode_enabled = true;

    log_out(LOG_START_SPACER"DEBUG_MODE"LOG_END_SPACER);

    _debug_mode_init();

    uint32_t prev_now = 0;
    while(platform_running() && (_debug_mode_enabled || !_debug_mode_init_get_toggle)) /* Do extra loop if waiting to collect sensors so not to confuse when rejoining measurements.*/
    {
        while(since_boot_delta(get_since_boot_ms(), prev_now) < 1000)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            _debug_mode_fast_iteration();
        }
        protocol_loop_iteration();
        prev_now = get_since_boot_ms();
        _debug_mode_iteration();
        platform_blink_led_toggle();
        platform_watchdog_reset();
    }
}


static command_response_t _debug_mode_cb(char* args)
{
    uint32_t mask = persist_get_log_debug_mask();
    if (mask & DEBUG_MODE)
    {
        debug_mode(); /* Toggle it off.*/
        persist_set_log_debug_mask(mask & ~DEBUG_MODE);
        return COMMAND_RESP_OK;
    }
    persist_set_log_debug_mask(mask | DEBUG_MODE);
    platform_raw_msg("Rebooting in debug_mode.");
    platform_reset_sys();
    /* Will never actually get to return anything, but GCC must be
     * satisfied.
     */
    return COMMAND_RESP_OK;
}


struct cmd_link_t* debug_mode_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "debug_mode",   "Set/unset debug mode",    _debug_mode_cb                 , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
