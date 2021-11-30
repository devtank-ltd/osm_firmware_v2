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
#define MEASUREMENTS__FMT_SIZE       5


typedef struct
{
    const uint8_t   uuid;
    const uint8_t   data_id;
    const char*     name;
    uint8_t         interval; // multiples of 5 mins
    value_t         value;
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

volatile bool measurement_trigger = false;
static char measurement_hex_str[MEASUREMENTS__STR_SIZE * LW__MAX_MEASUREMENTS] = {0};
uint32_t interval_count = 0;


void measurements_init(void)
{
    measurement_list_t data_template[] = { { MEASUREMENT_UUID__TEMP , LW_ID__TEMPERATURE  , "temperature"   ,  1, MEASUREMENTS__UNSET_VALUE } ,
                                           { MEASUREMENT_UUID__HUM  , LW_ID__HUMIDITY     , "humidity"      ,  1, MEASUREMENTS__UNSET_VALUE } ,
                                           { MEASUREMENT_UUID__PM10 , LW_ID__AIR_QUALITY  , "pm10"          ,  1, MEASUREMENTS__UNSET_VALUE } ,
                                           { MEASUREMENT_UUID__PM25 , LW_ID__AIR_QUALITY  , "pm25"          ,  1, MEASUREMENTS__UNSET_VALUE } };
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


static bool measurements_to_hex_str(volatile measurement_list_t* measurement)
{
    char fmt_tmp[MEASUREMENTS__FMT_SIZE] = {0};
    char meas[MEASUREMENTS__STR_SIZE] = {0};
    uint8_t sensor_id = 3;

    snprintf(meas, MEASUREMENTS__STR_SIZE, "%02x%02x", sensor_id, measurement->data_id);

    switch (measurement->data_id)
    {
        case LW_ID__TEMPERATURE:
            snprintf(fmt_tmp, MEASUREMENTS__FMT_SIZE, "%04"PRIx16, (uint16_t)measurement->value);
            strncat(meas, fmt_tmp, strlen(fmt_tmp));
            break;
        case LW_ID__HUMIDITY:
            snprintf(fmt_tmp, MEASUREMENTS__FMT_SIZE, "%02"PRIx8, (uint8_t)measurement->value);
            strncat(meas, fmt_tmp, strlen(fmt_tmp));
            break;
        case LW_ID__AIR_QUALITY:
            snprintf(fmt_tmp, MEASUREMENTS__FMT_SIZE, "%02"PRIx8, (uint8_t)measurement->uuid);
            strncat(meas, fmt_tmp, strlen(fmt_tmp));
            memset(fmt_tmp, 0, MEASUREMENTS__FMT_SIZE);
            snprintf(fmt_tmp, MEASUREMENTS__FMT_SIZE, "%04"PRIx16, (uint16_t)measurement->value);
            strncat(meas, fmt_tmp, strlen(fmt_tmp));
            break;
        default:
            printf("Unknown ID: %u", measurement->data_id);
            return false;
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
        if (interval_count % measurement->interval == 0)
        {
            if (measurement->value == MEASUREMENTS__UNSET_VALUE)
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
        lw_send(measurement_hex_str);
    }
}


bool measurements_read_data_value(uint8_t uuid, value_t* val)
{
    volatile measurement_list_t* measurement;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (measurement->uuid == uuid)
        {
            *val = measurement->value;
            return true;
        }
    }
    return false;
}


bool measurements_write_data_value(uint8_t uuid, value_t val)
{
    volatile measurement_list_t* measurement;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.write_data[i];
        if (measurement->uuid == uuid)
        {
            measurement->value = val;
            return true;
        }
    }
    return false;
}


void measurements_loop(void)
{
    if (measurement_trigger)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send(interval_count);
    }
}
