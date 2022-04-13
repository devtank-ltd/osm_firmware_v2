#include <inttypes.h>
#include <stddef.h>

#include "bat.h"

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "uart_rings.h"


#define BAT_MON_DEFAULT_COLLECTION_TIME     40
#define BAT_TIMEOUT_MS                      1000
#define BAT_NUM_SAMPLES                     20
#define BAT_IS_VALID_FOR_S                  120


#define BAT_MUL                             10000UL
#define BAT_MAX_MV                          1343 /* 1.343 volts */
#define BAT_MIN_MV                          791  /* 0.791 volts */
#define BAT_MAX                             (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MAX_MV)
#define BAT_MIN                             (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MIN_MV)
#define BAT_ON_BAT_THRESHOLD                BAT_MAX * 1.1


typedef struct
{
    bool        on_battery;
    uint32_t    last_checked;
} bat_on_battery_t;


static volatile bool    _bat_running            = false;
static uint32_t         _bat_collection_time    = BAT_MON_DEFAULT_COLLECTION_TIME;
static bat_on_battery_t _bat_on_battery         = {false, 0};


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


static void _cc_release(void)
{
    adcs_release(ADCS_KEY_BAT);
}


static void _bat_update_on_battery(bool on_battery)
{
    _bat_on_battery.on_battery = on_battery;
    _bat_on_battery.last_checked = get_since_boot_ms();
}


measurements_sensor_state_t bat_collection_time(char* name, uint32_t* collection_time)
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


measurements_sensor_state_t bat_begin(char* name)
{
    if (_bat_running)
    {
        adc_debug("Already beginning BAT ADC.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint8_t bat_channel = ADC1_CHANNEL_BAT_MON;
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


measurements_sensor_state_t bat_get(char* name, value_t* value)
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

    uint16_t raw16;
    adcs_resp_t resp = adcs_collect_avgs(&raw16, 1, BAT_NUM_SAMPLES, ADCS_KEY_BAT, &_bat_collection_time);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            _bat_running = false;
            _cc_release();
            adc_debug("ADC for Bat not complete!");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            _bat_running = false;
            _cc_release();
            break;
    }

    adc_debug("Bat raw ADC:%"PRIu16, raw16);

    uint32_t raw = raw16 * BAT_MUL;

    uint16_t perc;

    _bat_update_on_battery(raw <= BAT_ON_BAT_THRESHOLD);
    if (raw > BAT_MAX)
    {
        perc = BAT_MUL;
    }
    else if (raw < BAT_MIN)
    {
        /* How are we here? */
        perc = (uint16_t)BAT_MIN;
    }
    else
    {
        uint32_t divider =  (BAT_MAX / BAT_MUL) - (BAT_MIN / BAT_MUL);
        perc = (raw - BAT_MIN) / divider;
    }

    *value = value_from(perc);

    adc_debug("Bat %u.%02u", perc / 100, perc %100);

    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool bat_get_blocking(char* name, value_t* value)
{
    if (bat_begin(name) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Could not start BAT.");
        return false;
    }
    if (!_bat_wait())
    {
        adc_debug("BAT Wait failed.");
        return false;
    }
    if (!bat_get(name, value) == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Failed get BAT.");
        return false;
    }
    return true;
}


static bool _bat_get_on_battery(bool* on_battery)
{
    if (!on_battery)
        return false;

    if (!bat_begin(NULL))
        return false;
    _bat_wait();

    if (!_bat_running)
    {
        adc_debug("ADC for Bat not running!");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    _bat_running = false;

    uint16_t raw16;
    if (!adcs_collect_avgs(&raw16, 1, BAT_NUM_SAMPLES, ADCS_KEY_BAT, NULL))
    {
        adc_debug("ADC for Bat not complete!");
        return false;
    }
    *on_battery = raw16 < BAT_ON_BAT_THRESHOLD;
    _bat_update_on_battery(*on_battery);
    return true;
}


bool bat_on_battery(bool* on_battery)
{
    if ((since_boot_delta(get_since_boot_ms(), _bat_on_battery.last_checked) * 1000) <= BAT_IS_VALID_FOR_S)
    {
        *on_battery = _bat_on_battery.on_battery;
        return true;
    }
    return _bat_get_on_battery(on_battery);
}
