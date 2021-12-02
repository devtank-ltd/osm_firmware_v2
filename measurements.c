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
#include "hpm.h"
#include "sys_time.h"


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
    uint8_t         sample_rate;        // multiples of 1 minute. Must be greater than or equal to 1
    bool            (*cb)(value_t* value);
    value_t         sum;
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

static uint32_t last_sent_ms = 0;
static uint32_t interval_count = 0;
static uint16_t measurements_hex_arr[MEASUREMENTS__HEX_ARRAY_SIZE] = {0};
static uint16_t measurements_hex_arr_pos = 0;


static bool hpm_get_pm10(value_t* value);
static bool hpm_get_pm25(value_t* value);


measurement_list_t data_template[] = { { MEASUREMENT_UUID__PM10 , LW_ID__PM10  , "pm10"          ,  1, 1, hpm_get_pm10, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0} ,
                                       { MEASUREMENT_UUID__PM25 , LW_ID__PM25  , "pm25"          ,  1, 1, hpm_get_pm25, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0} };


static bool hpm_get_pm10(value_t* value)
{
    uint16_t dummy;
    return hpm_get((uint16_t*)value, &dummy);
}


static bool hpm_get_pm25(value_t* value)
{
    uint16_t dummy;
    return hpm_get(&dummy, (uint16_t*)value);
}


void measurements_init(void)
{
    memcpy((measurement_list_t*)data_0, data_template, sizeof(data_template));
    memcpy((measurement_list_t*)data_1, data_template, sizeof(data_template));
    data.read_data = (measurement_list_t*)data_0;
    data.write_data = (measurement_list_t*)data_1;
    data.len = ARRAY_SIZE(data_template);
}


static void measurements_copy(void)
{
    CM_ATOMIC_BLOCK()
    {
        memcpy(data.read_data, data.write_data, sizeof(data_template));
        memcpy(data.write_data, (measurement_list_t*)data_template, sizeof(data_template));
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
            *exponent = MEASUREMENTS__EXPONENT_TEMPERATURE;
            break;
        case LW_ID__HUMIDITY:
            *exponent = MEASUREMENTS__EXPONENT_HUMIDITY;
            break;
        case LW_ID__PM10:
            *exponent = MEASUREMENTS__EXPONENT_PM10;
            break;
        case LW_ID__PM25:
            *exponent = MEASUREMENTS__EXPONENT_PM25;
            break;
        default:
            return false;
    }
    return true;
}


static uint8_t measurements_arr_append(uint16_t val)
{
    if (measurements_hex_arr_pos >= LW__MAX_MEASUREMENTS)
    {
        log_error("Measurement array is full.");
        return 1;
    }
    measurements_hex_arr[measurements_hex_arr_pos++] = val;
    return 0;
}


static bool measurements_to_arr(volatile measurement_list_t* measurement)
{
    bool single = measurement->sample_rate == 1;
    uint32_t mean = measurement->sum / measurement->num_samples;
    int16_t exponent;

    if (!measurement_get_exponent(measurement->data_id, &exponent))
    {
        log_error("Cannot find exponent for %s (%"PRIu16").", measurement->name, measurement->data_id);
        return false;
    }

    uint8_t r = 0;
    r += measurements_arr_append(measurement->data_id);
    if (single)
    {
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_SINGLE);
        r += measurements_arr_append(exponent);
        r += measurements_arr_append(mean);
    }
    else
    {
        r += measurements_arr_append(MEASUREMENTS__DATATYPE_SINGLE);
        r += measurements_arr_append(exponent);
        r += measurements_arr_append(mean);
        r += measurements_arr_append(measurement->min);
        r += measurements_arr_append(measurement->max);
    }
    return (r == 0);
}


void measurements_send(void)
{
    measurements_copy();
    volatile measurement_list_t* measurement;
    memset(measurements_hex_arr, 0, LW__MAX_MEASUREMENTS);
    measurements_hex_arr_pos = 0;
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
            if (measurements_to_arr(measurement))
            {
                num_meas_qd++;
            }
        }
    }
    if (num_meas_qd > 0)
    {
        lw_send(measurements_hex_arr, measurements_hex_arr_pos+1);
    }
}


uint8_t a = 0;


static void measurements_sample(void)
{
    volatile measurement_list_t* measurement;
    uint32_t sample_interval;
    value_t new_value;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        sample_interval = measurement->interval * INTERVAL__SAMPLE_MS / measurement->sample_rate;
        if (since_boot_delta(since_boot_ms, last_sent_ms) < sample_interval/2)
        {
            continue;
        }
        if ((since_boot_delta(since_boot_ms, last_sent_ms)-sample_interval/2) % sample_interval == 0)
        {
            if (!measurement->cb(&new_value))
            {
                log_error("Could not get the %s value.", measurement->name);
                return;
            }
            if (measurement->sum == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->sum = 0;
            }
            if (measurement->min == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->min = 0;
            }
            if (measurement->max == MEASUREMENTS__UNSET_VALUE)
            {
                measurement->max = 0;
            }
            measurement->sum += new_value;
            measurement->num_samples++;
            if (new_value > measurement->max)
            {
                measurement->max = new_value;
            }
            else if (new_value < measurement->min)
            {
                measurement->min = new_value;
            }
            log_out("measurement->sum = %"PRIu64, measurement->sum);
            log_out("measurement->max = %"PRIu64, measurement->max);
            log_out("measurement->min = %"PRIu64, measurement->min);
        }
    }
    a = 1;
}


void measurements_loop(void)
{
    uint32_t now = since_boot_ms;
    measurements_sample();

    if (since_boot_delta(now, last_sent_ms) > INTERVAL__SAMPLE_MS)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send();
        last_sent_ms = now;
    }
}
