#include <inttypes.h>

#include "bat.h"

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "uart_rings.h"


#define BAT_MON_DEFAULT_COLLECTION_TIME     1000
#define BAT_TIMEOUT_MS                      1000


#define BAT_MUL     10000UL
#define BAT_MAX_MV  1343 /* 1.343 volts */
#define BAT_MIN_MV  791  /* 0.791 volts */
#define BAT_MAX     (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MAX_MV)
#define BAT_MIN     (ADC_MAX_VAL * BAT_MUL / ADC_MAX_MV * BAT_MIN_MV)


static volatile bool    _bat_running    = false;


static bool _bat_wait(void)
{
    adc_debug("Waiting for ADC BAT");
    if (!adcs_wait_done(BAT_TIMEOUT_MS))
    {
        adc_debug("Timed out waiting for BAT ADC.");
        return false;
    }
    return true;
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
    *collection_time = BAT_MON_DEFAULT_COLLECTION_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t bat_begin(char* name)
{
    if (_bat_running)
    {
        adc_debug("Already beginning BAT ADC.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    _bat_running = true;

    adc_debug("Started ADC reading for BAT.");
    uint8_t bat_channel = ADC1_CHANNEL_BAT_MON;
    if (!adcs_begin(&bat_channel, 1))
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t bat_get(char* name, value_t* value)
{
    if (!_bat_running)
    {
        adc_debug("ADC for Bat not running!");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    _bat_running = false;

    if (!value)
    {
        adc_debug("Handed NULL Pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint16_t raw16;
    if (!adcs_collect_avgs(&raw16, 1))
    {
        adc_debug("ADC for Bat not complete!");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }

    adc_debug("Bat raw ADC:%"PRIu16, raw16);

    uint32_t raw = raw16 * BAT_MUL;

    uint16_t perc;

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
