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
#include "persist_config.h"


#define MEASUREMENTS__UNSET_VALUE   UINT32_MAX
#define MEASUREMENTS__STR_SIZE      16

#define MEASUREMENTS__PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENTS__COLLECT_TIME__TEMPERATURE__MS 10000
#define MEASUREMENTS__COLLECT_TIME__HUMIDITY__MS    10000
#define MEASUREMENTS__COLLECT_TIME__HPM__MS         10000

#define MEASUREMENT_ID_FROM_NAME(_name_)  *(uint64_t*)(char[8]){_name_}

#define MEASUREMENT_PM10_NAME "pm10"
#define MEASUREMENT_PM10_ID   MEASUREMENT_ID_FROM_NAME(MEASUREMENT_PM10_NAME)

#define MEASUREMENT_PM25_NAME "pm25"
#define MEASUREMENT_PM25_ID  MEASUREMENT_ID_FROM_NAME(MEASUREMENT_PM25_NAME)


typedef struct
{
    value_t     sum;
    value_t     max;
    value_t     min;
    uint8_t     num_samples;
    uint8_t     num_samples_init;
    uint8_t     num_samples_collected;
} measurement_data_t;


typedef struct
{
    measurement_def_t   def[LW__MAX_MEASUREMENTS];
    measurement_data_t  data[LW__MAX_MEASUREMENTS];
    uint16_t            len;
} measurement_arr_t;


static uint32_t last_sent_ms = 0;
static uint32_t interval_count = 0;
static int8_t measurements_hex_arr[MEASUREMENTS__HEX_ARRAY_SIZE] = {0};
static uint16_t measurements_hex_arr_pos = 0;
static measurement_arr_t measurement_arr = {0};


uint32_t hpm_init_time = 0;
uint16_t hpm_pm10, hpm_pm25;


static uint32_t modbus_bus_collection_time(void) { return 10000; }
static bool modbus_init(char* name) { return true; }
static bool modbus_get(char* name, value_t* value) { return true; }


static void hpm_preinit(void)
{
    uint32_t now = since_boot_ms;
    if (hpm_init_time == 0 || since_boot_delta(now, hpm_init_time) > MEASUREMENTS__COLLECT_TIME__HPM__MS)
    {
        hpm_enable(true);
        hpm_request();
        hpm_init_time = now;
    }
}


static bool hpm_pm10_init(char* name)
{
    hpm_preinit();
    return true;
}


static bool hpm_pm10_get(char* name, value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm10;
    return true;
}


static bool hpm_pm25_init(char* name)
{
    hpm_preinit();
    return true;
}


static bool hpm_pm25_get(char* name, value_t* value)
{
    *value = 0;
    uint16_t hpm_pm10_cp, hpm_pm25_cp;
    if (hpm_get(&hpm_pm10_cp, &hpm_pm25_cp))
    {
        hpm_pm10 = hpm_pm10_cp;
        hpm_pm25 = hpm_pm25_cp;
    }
    *value = hpm_pm25;
    return true;
}


static bool measurements_get_measurement_def(char* name, measurement_def_t* measurement_def)
{
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        measurement_def = &measurement_arr.def[i];
        if (strncmp(measurement_def->base.name, name, strlen(measurement_def->base.name)) == 0)
        {
            return true;
        }
    }
    return false;
}


static bool measurements_arr_append_i8(int8_t val)
{
    if (measurements_hex_arr_pos >= MEASUREMENTS__HEX_ARRAY_SIZE)
    {
        log_error("Measurement array is full.");
        return false;
    }
    measurements_hex_arr[measurements_hex_arr_pos++] = val;
    return true;
}


static bool measurements_arr_append_i16(int16_t val)
{
    return measurements_arr_append_i8(val & 0xFF) &&
           measurements_arr_append_i8((val >> 8) & 0xFF);
}
    

static bool measurements_arr_append_i32(int32_t val)
{
    return measurements_arr_append_i16(val & 0xFFFF) &&
           measurements_arr_append_i16((val >> 16) & 0xFFFF);
}


static bool measurements_arr_append_i64(int64_t val)
{
    return measurements_arr_append_i32(val & 0xFFFFFFFF) &&
           measurements_arr_append_i32((val >> 32) & 0xFFFFFFFF);
}



#define measurements_arr_append(_b_) _Generic((_b_),                       \
                                    signed char: measurements_arr_append_i8,   \
                                    short int: measurements_arr_append_i16, \
                                    long int: measurements_arr_append_i32, \
                                    long long int: measurements_arr_append_i64)(_b_)


static bool measurements_to_arr(measurement_def_t* measurement_def, measurement_data_t* measurement_data)
{
    bool single = measurement_def->base.samplecount == 1;
    int16_t mean = measurement_data->sum / measurement_data->num_samples;

    bool r = 0;
    r |= !measurements_arr_append(*(int64_t*)measurement_def->base.name);
    if (single)
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS__DATATYPE_SINGLE);
        r |= !measurements_arr_append((int16_t)mean);
    }
    else
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS__DATATYPE_AVERAGED);
        r |= !measurements_arr_append((int16_t)mean);
        r |= !measurements_arr_append((int16_t)measurement_data->min);
        r |= !measurements_arr_append((int16_t)measurement_data->max);
    }
    return r;
}


static void measurements_send(void)
{
    uint16_t            num_qd = 0;
    measurement_def_t*  m_def;
    measurement_data_t* m_data;

    memset(measurements_hex_arr, 0, LW__MAX_MEASUREMENTS);
    measurements_hex_arr_pos = 0;

    if (!measurements_arr_append((int8_t)MEASUREMENTS__PAYLOAD_VERSION))
    {
        return;
    }

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];
        m_data->num_samples_init = 0;
        m_data->num_samples_collected = 0;
        if (m_def->base.interval && (interval_count % m_def->base.interval == 0))
        {
            if (m_data->sum == MEASUREMENTS__UNSET_VALUE || m_data->num_samples == 0)
            {
                log_error("Measurement requested but value not set.");
                continue;
            }
            if (measurements_to_arr(m_def, m_data))
            {
                num_qd++;
            }
        }
    }
    if (num_qd > 0)
    {
        lw_send(measurements_hex_arr, measurements_hex_arr_pos+1);
    }
}


static void measurements_sample(void)
{
    measurement_def_t*  m_def;
    measurement_data_t* m_data;
    uint32_t            sample_interval;
    value_t             new_value;
    uint32_t            now = since_boot_ms;
    uint32_t            time_since_interval;

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def  = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];

        sample_interval = m_def->base.interval * INTERVAL__TRANSMIT_MS / m_def->base.samplecount;
        time_since_interval = since_boot_delta(now, last_sent_ms);

        // If it takes time to get a sample, it is begun here.
        if (time_since_interval >= (m_data->num_samples_init * sample_interval) + sample_interval/2 - m_def->collection_time)
        {
            m_data->num_samples_init++;
            if (!m_def->init_cb(m_def->base.name))
            {
                log_error("Failed to call init for %s.", m_def->base.name);
                m_data->num_samples_collected++;
            }
        }

        // The sample is collected every interval/samplecount but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^
        if (time_since_interval >= (m_data->num_samples_collected * sample_interval) + sample_interval/2)
        {
            m_data->num_samples_collected++;
            if (!m_def->get_cb(m_def->base.name, &new_value))
            {
                log_error("Could not get the %s value.", m_def->base.name);
                return;
            }
            if (m_data->sum == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->sum = 0;
            }
            if (m_data->min == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->min = new_value;
            }
            if (m_data->max == MEASUREMENTS__UNSET_VALUE)
            {
                m_data->max = new_value;
            }
            m_data->sum += new_value;
            m_data->num_samples++;
            if (new_value > m_data->max)
            {
                m_data->max = new_value;
            }
            else if (new_value < m_data->min)
            {
                m_data->min = new_value;
            }
            log_debug(DEBUG_MEASUREMENTS, "New %s reading: %"PRIi64, m_def->base.name, new_value);
            log_debug(DEBUG_MEASUREMENTS, "%s sum: %"PRIi64, m_def->base.name, m_data->sum);
            log_debug(DEBUG_MEASUREMENTS, "%s min: %"PRIi64, m_def->base.name, m_data->min);
            log_debug(DEBUG_MEASUREMENTS, "%s max: %"PRIi64, m_def->base.name, m_data->max);
        }
    }
}


uint16_t measurements_num_measurements(void)
{
    return measurement_arr.len;
}


bool measurements_add(measurement_def_t* measurement_def)
{
    if (measurement_arr.len >= LW__MAX_MEASUREMENTS)
    {
        log_error("Cannot add more measurements. Reached max.");
        return false;
    }
    measurement_def_t* measurement_def_iter;
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        measurement_def_iter = &measurement_arr.def[i];
        if (strncmp(measurement_def_iter->base.name, measurement_def->base.name, sizeof(measurement_def_iter->base.name)) == 0)
        {
            log_error("Tried to add measurement with the same name: %s", measurement_def->base.name);
            return false;
        }
    }
    measurement_data_t measurement_data = { MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, MEASUREMENTS__UNSET_VALUE, 0, 0, 0};
    measurement_arr.def[measurement_arr.len] = *measurement_def;
    measurement_arr.data[measurement_arr.len] = measurement_data;
    measurement_arr.len++;
    return true;
}


bool measurements_del(char* name)
{
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        if (strcmp(name, measurement_arr.def[i].base.name) == 0)
        {
            memset(&measurement_arr.def[measurement_arr.len], 0, sizeof(measurement_def_t));
            measurement_arr.len--;
            return true;
        }
    }
    log_error("Cannot find measurement to remove.");
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurement_def_t* measurement_def = NULL;
    if (!measurements_get_measurement_def(name, measurement_def))
    {
        return false;
    }
    measurement_def->base.interval = interval;
    if (!persist_set_interval(name, interval))
    {
        log_error("Could not write interval to persistent storage.");
    }
    return true;
}


bool measurements_set_samplecount(char* name, uint8_t samplecount)
{
    if (samplecount == 0)
    {
        log_error("Cannot set the samplecount to 0.");
        return false;
    }
    measurement_def_t* measurement_def = NULL;
    if (!measurements_get_measurement_def(name, measurement_def))
    {
        return false;
    }
    measurement_def->base.samplecount = samplecount;
    if (!persist_set_samplecount(name, samplecount))
    {
        log_error("Could not write samplecount to persistent storage.");
    }
    return true;
}


void measurements_loop_iteration(void)
{
    uint32_t now = since_boot_ms;
    measurements_sample();
    if (since_boot_delta(now, last_sent_ms) > INTERVAL__TRANSMIT_MS)
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


bool measurements_save(void)
{
    measurement_def_base_t* persistent_measurement_arr;
    if (!persist_get_measurements(&persistent_measurement_arr))
    {
        return false;
    }
    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        measurement_def_base_t* def_base = &measurement_arr.def[i].base;
        persistent_measurement_arr[i] = *def_base;
    }
    persist_commit_measurement();
    return true;
}



static void _measurement_fixup(measurement_def_t * def)
{
    switch(def->base.type)
    {
        case PM10:
            def->collection_time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            def->init_cb = hpm_pm10_init;
            def->get_cb = hpm_pm10_get;
            break;
        case PM25:
            def->collection_time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            def->init_cb = hpm_pm25_init;
            def->get_cb = hpm_pm25_get;
            break;
        case MODBUS:
            def->collection_time = modbus_bus_collection_time();
            def->init_cb = modbus_init;
            def->get_cb = modbus_get;
            break;
    }
}



void measurements_init(void)
{
    measurement_def_base_t* persistent_measurement_arr;
    if (!persist_get_measurements(&persistent_measurement_arr))
    {
        /* Add defaults. */
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_PM10_NAME, 8);
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 5;
            temp_def.base.type = PM10;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_PM10_NAME, 8);
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 5;
            temp_def.base.type = PM25;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        measurements_save();
        return;
    }

    for(unsigned n = 0; n < LW__MAX_MEASUREMENTS; n++)
    {
        measurement_def_base_t * def_base = &persistent_measurement_arr[n];

        char id_start = def_base->name[0];

        if (!id_start || id_start == 0xFF)
            break;

        measurement_def_t new_def;

        new_def.base = *def_base;

        _measurement_fixup(&new_def);

        measurements_add(&new_def);
    }

    /*
     * 
     */
}
