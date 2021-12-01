#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/cm3/cortex.h>

#include "measurements.h"
#include "lorawan.h"
#include "log.h"
#include "config.h"


#define MEASUREMENTS__UNSET_VALUE   UINT32_MAX
#define MEASUREMENTS__STR_SIZE      16

#define MEASUREMENTS__PAYLOAD_VERSION       0x01
#define MEASUREMENTS__DATATYPE_SINGLE       0x01
#define MEASUREMENTS__DATATYPE_AVERAGED     0x02


#define MEASUREMENTS__EXPONENT_TEMPERATURE      -3
#define MEASUREMENTS__EXPONENT_HUMIDITY         -1
#define MEASUREMENTS__EXPONENT_PM10              0
#define MEASUREMENTS__EXPONENT_PM25              0

typedef struct
{
    const uint8_t   uuid;
    const uint16_t  data_id;
    const char*     name;
    uint8_t         interval;           // multiples of 5 mins
    uint8_t         sample_rate;        // multiples of 1 minute
    bool            (*cb)(value_t* value);
    value_t         value;
    value_t         max;
    value_t         min;
    uint8_t         num_samples;
} measurement_list_t;


typedef struct
{
    measurement_list_t*     read_data;
    measurement_list_t*     write_data;
    uint16_t                len;
} data_structure_t;


static measurement_list_t data_0[LW__MAX_MEASUREMENTS];
static measurement_list_t data_1[LW__MAX_MEASUREMENTS];

static data_structure_t data;

volatile bool measurement_trigger = true;
static char measurement_hex_str[MEASUREMENTS__STR_SIZE * LW__MAX_MEASUREMENTS] = {0};
uint32_t last_sent_ms = 0;
uint32_t last_sampled_ms = 0;
uint32_t interval_count = 0;


measurement_list_t data_template[] = { { MEASUREMENT_UUID__PM10 , LW_ID__AIR_QUALITY  , "pm10"          ,  1, 1, hpm_get_pm10, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE } ,
                                       { MEASUREMENT_UUID__PM25 , LW_ID__AIR_QUALITY  , "pm25"          ,  1, 1, hpm_get_pm25, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE } };


bool hpm_get_pm10(value_t* value)
{
    uint16_t dummy;
    return hpm_get(value, &dummy);
}


bool hpm_get_pm25(value_t* value)
{
    uint16_t dummy;
    return hpm_get(&dummy, value);
}


void measurements_init(void)
{
    memcpy((measurement_list_t*)&data_0, &data_template, sizeof(data_template));
    memcpy((measurement_list_t*)&data_1, &data_template, sizeof(data_template));
    data.read_data = (measurement_list_t*)data_0;
    data.write_data = (measurement_list_t*)data_1;
    data.len = ARRAY_SIZE(data_template);
}


static void measurements_copy(void)
{
    CM_ATOMIC_BLOCK()
    {
        memcpy(&data.read_data, &data.write_data, sizeof(data.write_data));
        memcpy(&data.write_data, &data_template, sizeof(data_template));
    }
}


bool measurements_set_interval_uuid(uint8_t uuid, uint8_t interval)
{
    volatile measurement_list_t* measurement;
    for (uint8_t i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (measurement->uuid == uuid)
        {
            measurement->interval = interval;
            return true;
        }
    }
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    volatile measurement_list_t* measurement;
    for (uint8_t i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (strncmp(measurement->name, name, strlen(measurement->name)) == 0)
        {
            measurement->interval = interval;
            return true;
        }
    }
    return false;
}



static bool measurement_get_exponent(uint16_t data_id, int16_t* exponent)
{
    switch (data_id)
    {
        case LW_ID__TEMPERATURE:
            exponent = MEASUREMENTS__EXPONENT_TEMPERATURE;
            break;
        case LW_ID__HUMIDITY:
            exponent = MEASUREMENTS__EXPONENT_HUMIDITY;
            break;
        case LW_ID__PM10:
            exponent = MEASUREMENTS__EXPONENT_PM10;
            break;
        case LW_ID__PM25:
            exponent = MEASUREMENTS__EXPONENT_PM25;
            break;
        default:
            return false;
    }
    return true;
}


static bool measurements_to_hex_str(volatile measurement_list_t* measurement)
{
    char meas[MEASUREMENTS__STR_SIZE] = {0};
    bool single = measurement->sample_interval == 1;
    uint32_t mean = measurement->sum / measurement->num_samples;
    int16_t exponent;

    if (!measurement_get_exponent(measurement->data_id, &exponent))
    {
        log_error("Cannot find exponent for %s.", measurement->name);
        return false;
    }
    if (single)
    {
        snprintf(meas, "%02x%02x%02x%04x", measurement->data_id, MEASUREMENTS__DATATYPE_SINGLE, exponent, mean);
    }
    else
    {
        snprintf(meas, "%02x%02x%02x%04x%04x%04x", measurement->data_id, MEASUREMENTS__DATATYPE_AVERAGED, exponent, mean, measurement->min, measurement->max);
    }
    strncat(measurement_hex_str, meas, MEASUREMENTS__STR_SIZE);
    return true;
}


void measurements_send(uint32_t interval_count)
{
    measurements_copy();
    volatile measurement_list_t* measurement;
    memset(measurement_hex_str, 0, MEASUREMENTS__STR_SIZE * LW__MAX_MEASUREMENTS);
    uint16_t num_meas_qd = 0;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.read_data[i];
        if (measurement->interval && (interval_count % measurement->interval == 0))
        {
            if (measurement->sum == MEASUREMENTS__UNSET_VALUE || measurement->num_samples == 0)
            {
                log_error("Measurement requested but value not set.");
                continue;
            }
            if (measurements_to_hex_str(measurement))
            {
                num_meas_qd++;
            }
        }
    }
    if (num_meas_qd > 0)
    {
        lw_send(measurement_hex_str); // TODO: Make this be an array of uint16_ts instead of string.
    }
}


void measurements_update(void)
{
    volatile measurement_list_t* measurement;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (measurement->sum == MEASUREMENTS__UNSET_VALUE || measurement->num_samples == 0)
        {
            value_t value;
            if (!measurement->cb(&value))
            {
                log_error("Could not get the %s value.", measurement->name);
                return;
            }
            measurement->sum = value;
            measurement->num_samples++;
        }
        else if (measurement->sample_interval && (interval % measurement->sample_interval == 0))
        {
            value_t new_value;
            if (!measurement->cb(&new_value))
            {
                log_error("Could not get the %s value.", measurement->name);
                return;
            }
            measurement->sum += new_value;
            measurement->num_samples++;
            if (new_value > measurement->max)
            {
                measurement->max = measurement->sum;
            }
            else if (new_value < measurement->min)
            {
                measurement->min = measurement->sum;
            }
        }
    }
}


void measurements_loop(void)
{
    if (since_boot_delta(since_boot_ms, last_sampled_ms) > INTERVAL__TRANSMIT_MS)
    {
        measurements_update();
        last_sampled_ms = since_boot_ms;
    }

    if (since_boot_delta(since_boot_ms, last_sent_ms) > INTERVAL__SAMPLE_MS)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send(interval_count);
        last_sent_ms = since_boot_ms;
        last_sampled_ms = since_boot_ms;
    }
}
