#include <inttypes.h>
#include <stddef.h>

#include "bat.h"

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "persist_config.h"
#include "uart_rings.h"


#define BAT_MON_DEFAULT_COLLECTION_TIME     1000
#define BAT_TIMEOUT_MS                      1000
#define BAT_NUM_SAMPLES                     ADCS_NUM_SAMPLES
#define BAT_IS_VALID_FOR_MS                 120 * 1000


#define BAT_MUL                             10000UL
#define BAT_MAX_MV                          1343 /* 1.343 volts */
#define BAT_MIN_MV                          791  /* 0.791 volts */
#define BAT_MAX                             (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MAX_MV)
#define BAT_MIN                             (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MIN_MV)
#define BAT_ON_BAT_THRESHOLD                9000UL /* 90.00% */


typedef struct
{
    bool        on_battery;
    bool        is_valid;
    uint32_t    last_checked;
} bat_on_battery_t;


static bool             _bat_running            = false;
static uint32_t         _bat_starting_time      = 0;
static uint32_t         _bat_collection_time    = BAT_MON_DEFAULT_COLLECTION_TIME;
static bat_on_battery_t _bat_on_battery         = {false, false, 0};


static bool _bat_wait(void)
{
    adc_debug("Waiting for ADC BAT");
    adcs_resp_t resp = adcs_wait_done(BAT_TIMEOUT_MS, ADCS_KEY_BAT);
    switch (resp)
    {
        case ADCS_RESP_FAIL:
            break;
        case ADCS_RESP_WAIT:
            break;
        case ADCS_RESP_OK:
            return true;
    }
    adc_debug("Timed out waiting for BAT ADC.");
    return false;
}


static void _bat_release(void)
{
    adcs_release(ADCS_KEY_BAT);
}


static void _bat_update_on_battery(bool on_battery)
{
    _bat_on_battery.on_battery = on_battery;
    _bat_on_battery.is_valid = true;
    _bat_on_battery.last_checked = get_since_boot_ms();
    adc_debug("BAT status updated.");
}


static bool _bat_check_request(void)
{
    if (since_boot_delta(get_since_boot_ms(), _bat_starting_time) > BAT_TIMEOUT_MS)
    {
        adc_debug("BAT Request timed out.");
        _bat_running = false;
        _bat_release();
        return false;
    }
    return true;
}


static uint16_t _bat_conv(uint32_t raw)
{
    uint32_t scale = BAT_MUL / 1000;
    uint32_t raw32 = raw * scale;
    uint16_t perc;
    if (raw32 > BAT_MAX)
    {
        perc = BAT_MUL;
    }
    else if (raw32 < BAT_MIN)
    {
        /* How are we here? */
        perc = 0;
    }
    else
    {
        uint32_t divider =  (BAT_MAX / BAT_MUL) - (BAT_MIN / BAT_MUL);
        perc = (raw32 - BAT_MIN) / divider;
    }
    return perc;
}


static measurements_sensor_state_t _bat_collection_time_cb(char* name, uint32_t* collection_time)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = _bat_collection_time * 1.1;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _bat_begin(char* name, bool in_isolation)
{
    if (_bat_running && _bat_check_request())
    {
        adc_debug("Already beginning BAT ADC.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    adcs_type_t bat_channel = ADCS_TYPE_BAT;
    adcs_resp_t resp = adcs_begin(&bat_channel, 1, BAT_NUM_SAMPLES, ADCS_KEY_BAT);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Failed to begin BAT ADC.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            break;
    }
    adc_debug("Started ADC reading for BAT.");
    _bat_running = true;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _bat_get(char* name, measurements_reading_t* value)
{
    if (!_bat_running)
    {
        adc_debug("ADC for Bat not running!");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!value)
    {
        adc_debug("Handed NULL Pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint32_t raw;
    adcs_resp_t resp = adcs_collect_avgs(&raw, 1, BAT_NUM_SAMPLES, ADCS_KEY_BAT, &_bat_collection_time);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            _bat_running = false;
            _bat_release();
            adc_debug("ADC for Bat not complete!");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            _bat_running = false;
            _bat_release();
            break;
    }

    adc_debug("Bat raw ADC:%"PRIu32".%03"PRIu32, raw/1000, raw%1000);

    uint16_t perc = _bat_conv(raw);

    _bat_update_on_battery(perc < BAT_ON_BAT_THRESHOLD);

    value->v_f32 = to_f32_from_float((float)perc / 100.f);

    adc_debug("Bat %u.%02u", perc / 100, perc %100);

    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool bat_get_blocking(char* name, measurements_reading_t* value)
{
    if (_bat_begin(name, true) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Could not start BAT.");
        return false;
    }
    if (!_bat_wait())
    {
        adc_debug("BAT Wait failed.");
        return false;
    }
    if (!_bat_get(name, value) == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Failed get BAT.");
        return false;
    }
    return true;
}


static bool _bat_get_on_battery(bool* on_battery)
{
    if (!on_battery)
    {
        adc_debug("Handed a NULL pointer.");
        return false;
    }

    measurements_sensor_state_t resp = _bat_begin(NULL, true);
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            break;
        default:
            return false;
    }
    if (!_bat_wait())
    {
        adc_debug("Couldn't wait for the battery adc value to be ready.");
        return false;
    }

    if (!_bat_running)
    {
        adc_debug("ADC for Bat not running!");
        return false;
    }

    _bat_running = false;

    uint32_t raw;
    adcs_resp_t adc_resp = adcs_collect_avgs(&raw, 1, BAT_NUM_SAMPLES, ADCS_KEY_BAT, NULL);
    _bat_release();
    switch (adc_resp)
    {
        case ADCS_RESP_OK:
            break;
        default:
            adc_debug("ADC for Bat not complete!");
            return false;
    }
    uint16_t perc = _bat_conv(raw);
    *on_battery = perc < BAT_ON_BAT_THRESHOLD;
    _bat_update_on_battery(*on_battery);
    return true;
}


bool bat_on_battery(bool* on_battery)
{
    if (!on_battery)
    {
        adc_debug("Handed a NULL pointer.");
        return false;
    }

    if (_bat_on_battery.is_valid &&
        since_boot_delta(get_since_boot_ms(), _bat_on_battery.last_checked) <= BAT_IS_VALID_FOR_MS)
    {
        *on_battery = _bat_on_battery.on_battery;
        return true;
    }
    adc_debug("Valid battery status timed out. Getting new one.");
    if (!_bat_get_on_battery(on_battery))
    {
        /* Assumes on battery */
        _bat_update_on_battery(true);
        *on_battery = true;
    }
    return true;
}


static measurements_value_type_t _bat_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void                         bat_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _bat_collection_time_cb;
    inf->init_cb            = _bat_begin;
    inf->get_cb             = _bat_get;
    inf->value_type_cb      = _bat_value_type;
}


static command_response_t _bat_cb(char* args)
{
    measurements_reading_t value;
    if (!bat_get_blocking(NULL, &value))
    {
        log_out("Could not get bat value.");
        return COMMAND_RESP_ERR;
    }

    log_out("Bat %"PRIi64".%02"PRIi64, value.v_i64 / 100, value.v_i64 %100);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* bat_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = { { "bat",          "Get battery level.",      _bat_cb                        , false , NULL } };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
