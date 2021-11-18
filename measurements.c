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


volatile bool measurement_trigger = false;
static data_structure_t data;
static char meas_hex_str[MEASUREMENTS__STR_SIZE * LW__MAX_MEASUREMENTS] = {0};


void measurements_init(void)
{
    measurement_list_t data_template[] = { { MEASUREMENT_UUID__TEMP , LW_ID__TEMPERATURE  , "temperature"   ,  1, MEASUREMENTS__UNSET_VALUE } ,
                                           { MEASUREMENT_UUID__HUM  , LW_ID__HUMIDITY     , "humidity"      ,  1, MEASUREMENTS__UNSET_VALUE } };

    uint16_t len = ARRAY_SIZE(data_template);

    measurement_list_t data_0[len];
    measurement_list_t data_1[len];

    memcpy((measurement_list_t*)&data_0, &data_template, sizeof(data_template));
    memcpy((measurement_list_t*)&data_1, &data_template, sizeof(data_template));
    data.read_data = (measurement_list_t*)data_0;
    data.write_data = (measurement_list_t*)data_1;
    data.len = len;
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


static void measurements_to_hex_str(volatile measurement_list_t* measurement)
{
    char fmt[25] = "";
    char meas[MEASUREMENTS__STR_SIZE];
    uint8_t sensor_id = 0;
    uint8_t data_size;
    switch (measurement->data_id)
    {
        case LW_ID__TEMPERATURE:
            data_size = 2;
            break;
        default:
            data_size++;
            log_out("Unknown ID: %u", measurement->data_id);
            return;
    }
    strcpy(fmt, "%02x%02x%0"STR(data_size * 2)"x");
    snprintf(meas, MEASUREMENTS__STR_SIZE, fmt, sensor_id, measurement->data_id, measurement->value);
    strncat(meas_hex_str, meas, MEASUREMENTS__STR_SIZE);
}



static void measurements_send(uint32_t interval_count)
{
    measurements_copy();
    volatile measurement_list_t* measurement;
    memset(meas_hex_str, 0, MEASUREMENTS__STR_SIZE * LW__MAX_MEASUREMENTS);
    uint16_t num_meas_qd = 0;
    for (int i = 0; i < data.len; i++)
    {
        measurement = &data.read_data[i];
        if (interval_count % measurement->interval == 0)
        {
            num_meas_qd++;
            measurements_to_hex_str(measurement);
        }
    }
    if (num_meas_qd > 0)
    {
        lw_send(meas_hex_str);
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
    uint32_t interval_count = 0;
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