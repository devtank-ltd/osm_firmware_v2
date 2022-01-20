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
#include "adcs.h"
#include "sys_time.h"
#include "persist_config.h"
#include "modbus_measurements.h"
#include "one_wire_driver.h"


#define MEASUREMENTS__UNSET_VALUE   UINT32_MAX
#define MEASUREMENTS__STR_SIZE      16

#define MEASUREMENTS__PAYLOAD_VERSION       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_SINGLE       (uint8_t)0x01
#define MEASUREMENTS__DATATYPE_AVERAGED     (uint8_t)0x02

#define MEASUREMENT_PM10_NAME "PM10"
#define MEASUREMENT_PM10_ID   ID_FROM_NAME(MEASUREMENT_PM10_NAME)

#define MEASUREMENT_PM25_NAME "PM25"
#define MEASUREMENT_PM25_ID  ID_FROM_NAME(MEASUREMENT_PM25_NAME)

#define MEASUREMENT_CURRENT_CLAMP_NAME "CC1"
#define MEASUREMENT_CURRENT_CLAMP_ID  ID_FROM_NAME(MEASUREMENT_CURRENT_CLAMP_NAME)

#define MEASUREMENT_W1_PROBE_NAME "TMP2"
#define MEASUREMENT_W1_PROBE_ID  ID_FROM_NAME(MEASUREMENT_W1_PROBE_NAME)


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
    measurement_def_t   def[MEASUREMENTS_MAX_NUMBER];
    measurement_data_t  data[MEASUREMENTS_MAX_NUMBER];
    uint16_t            len;
} measurement_arr_t;


typedef struct
{
    uint32_t last_checked_time;
    uint32_t wait_time;
} measurement_check_time_t;


static uint32_t                 last_sent_ms                                        = 0;
static measurement_check_time_t check_time                                          = {0, 0};
static uint32_t                 interval_count                                      =  0;
static int8_t                   measurements_hex_arr[MEASUREMENTS__HEX_ARRAY_SIZE]  = {0};
static uint16_t                 measurements_hex_arr_pos                            =  0;
static measurement_arr_t        measurement_arr                                     = {0};



static bool measurements_get_measurement_def(char* name, measurement_def_t** measurement_def)
{
    measurement_def_t* t_measurement_def;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        t_measurement_def = &measurement_arr.def[i];
        if (ID_FROM_NAME(*t_measurement_def->base.name) == ID_FROM_NAME(*name))
        {
            *measurement_def = t_measurement_def;
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

static bool measurements_arr_append_value(value_t * value)
{
    if (!value)
        return false;
    if (!measurements_arr_append_i8(value->type))
        return false;
    switch(value->type)
    {
        case VALUE_UINT8  : return measurements_arr_append_i8(value->i8);
        case VALUE_INT8   : return measurements_arr_append_i8(value->i8);
        case VALUE_UINT16 : return measurements_arr_append_i16(value->i16);
        case VALUE_INT16  : return measurements_arr_append_i16(value->i16);
        case VALUE_UINT32 : return measurements_arr_append_i32(value->i32);
        case VALUE_INT32  : return measurements_arr_append_i32(value->i32);
        case VALUE_UINT64 : return measurements_arr_append_i64(value->i64);
        case VALUE_INT64  : return measurements_arr_append_i64(value->i64);
        case VALUE_FLOAT  : return measurements_arr_append_i32(value->i32);
        case VALUE_DOUBLE : return measurements_arr_append_i64(value->i64);
        default: break;
    }
    return false;
}

#define measurements_arr_append(_b_) _Generic((_b_),                            \
                                    signed char: measurements_arr_append_i8,    \
                                    short int: measurements_arr_append_i16,     \
                                    long int: measurements_arr_append_i32,      \
                                    long long int: measurements_arr_append_i64, \
                                    value_t * : measurements_arr_append_value)(_b_)


static bool measurements_to_arr(measurement_def_t* measurement_def, measurement_data_t* measurement_data)
{
    bool single = measurement_def->base.samplecount == 1;

    value_t mean;
    value_t num_samples = value_from(measurement_data->num_samples);
    if (!value_div(&mean, &measurement_data->sum, &num_samples))
    {
        log_error("Failed to average %s value.", measurement_def->base.name);
    }

    bool r = 0;
    r |= !measurements_arr_append(*(int32_t*)measurement_def->base.name);
    if (single)
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS__DATATYPE_SINGLE);
        r |= !measurements_arr_append(&mean);
    }
    else
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS__DATATYPE_AVERAGED);
        r |= !measurements_arr_append(&mean);
        r |= !measurements_arr_append(&measurement_data->min);
        r |= !measurements_arr_append(&measurement_data->max);
    }
    return !r;
}


static void measurements_send(void)
{
    uint16_t            num_qd = 0;
    measurement_def_t*  m_def;
    measurement_data_t* m_data;

    memset(measurements_hex_arr, 0, MEASUREMENTS_MAX_NUMBER);
    measurements_hex_arr_pos = 0;

    log_debug(DEBUG_MEASUREMENTS, "Attempting to send measurements");

    if (!measurements_arr_append((int8_t)MEASUREMENTS__PAYLOAD_VERSION))
    {
        return;
    }

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];
        if (m_def->base.interval && (interval_count % m_def->base.interval == 0))
        {
            if (m_data->sum.type == VALUE_UNSET || m_data->num_samples == 0)
            {
                log_error("Measurement requested but value not set.");
                continue;
            }
            if (!measurements_to_arr(m_def, m_data))
            {
                return;
            }
            num_qd++;
            m_data->sum = VALUE_EMPTY;
            m_data->min = VALUE_EMPTY;
            m_data->max = VALUE_EMPTY;
            m_data->num_samples = 0;
            m_data->num_samples_init = 0;
            m_data->num_samples_collected = 0;
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
    value_t             new_value = VALUE_EMPTY;
    uint32_t            now = since_boot_ms;
    uint32_t            time_since_interval;

    uint32_t            time_init;
    uint32_t            time_collect;

    check_time.last_checked_time = now;

    for (unsigned i = 0; i < measurement_arr.len; i++)
    {
        m_def  = &measurement_arr.def[i];
        m_data = &measurement_arr.data[i];

        sample_interval = m_def->base.interval * INTERVAL__TRANSMIT_MS / m_def->base.samplecount;
        time_since_interval = since_boot_delta(now, last_sent_ms);

        // If it takes time to get a sample, it is begun here.
        time_init = (m_data->num_samples_init * sample_interval) + sample_interval/2 - m_def->collection_time;
        if (time_since_interval >= time_init)
        {
            m_data->num_samples_init++;
            if (!m_def->init_cb(m_def->base.name))
            {
                log_error("Failed to call init for %s.", m_def->base.name);
                m_data->num_samples_collected++;
            }
        }
        else if (check_time.wait_time > (time_since_interval - time_init))
        {
            check_time.wait_time = time_since_interval - time_init;
        }

        // The sample is collected every interval/samplecount but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^
        time_collect = (m_data->num_samples_collected * sample_interval) + sample_interval/2;
        if (time_since_interval >= time_collect)
        {
            m_data->num_samples_collected++;
            new_value = VALUE_EMPTY;
            if (!m_def->get_cb(m_def->base.name, &new_value))
            {
                log_error("Could not get the %s value.", m_def->base.name);
                return;
            }
            if (m_data->sum.type == VALUE_UNSET)
            {
                m_data->sum.u64 = 0;
                m_data->sum.type = VALUE_INT32;
            }
            if (m_data->min.type == VALUE_UNSET)
            {
                m_data->min = new_value;
            }
            if (m_data->max.type == VALUE_UNSET)
            {
                m_data->max = new_value;
            }
            if (!value_add(&m_data->sum, &m_data->sum, &new_value))
            {
                log_error("Failed to add %s value.", m_def->base.name);
            }
            m_data->num_samples++;
            if (value_grt(&new_value, &m_data->max))
            {
                m_data->max = new_value;
            }
            else if (value_lst(&new_value, &m_data->min))
            {
                m_data->min = new_value;
            }
            log_debug(DEBUG_MEASUREMENTS, "New %s reading", m_def->base.name);
            log_debug_value(DEBUG_MEASUREMENTS, "Value :", &new_value);
            log_debug_value(DEBUG_MEASUREMENTS, "Sum :", &m_data->sum);
            log_debug_value(DEBUG_MEASUREMENTS, "Min :", &m_data->min);
            log_debug_value(DEBUG_MEASUREMENTS, "Max :", &m_data->max);
        }
        else if (check_time.wait_time > (time_since_interval - time_collect))
        {
            check_time.wait_time = time_since_interval - time_collect;
        }
    }
}


uint16_t measurements_num_measurements(void)
{
    return measurement_arr.len;
}


bool measurements_add(measurement_def_t* measurement_def)
{
    if (measurement_arr.len >= MEASUREMENTS_MAX_NUMBER)
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
    measurement_data_t measurement_data = { VALUE_EMPTY, VALUE_EMPTY, VALUE_EMPTY, 0, 0, 0};
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
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurement_def_t* measurement_def = NULL;
    if (!measurements_get_measurement_def(name, &measurement_def))
    {
        return false;
    }
    measurement_def->base.interval = interval;
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
    if (!measurements_get_measurement_def(name, &measurement_def))
    {
        return false;
    }
    measurement_def->base.samplecount = samplecount;
    return true;
}


void measurements_loop_iteration(void)
{
    uint32_t now = since_boot_ms;
    if (since_boot_delta(now, check_time.last_checked_time) > check_time.wait_time)
    {
        measurements_sample();
    }
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


static void _measurement_fixup(measurement_def_t* def)
{
    switch(def->base.type)
    {
        case PM10:
            def->collection_time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            def->init_cb = hpm_init;
            def->get_cb = hpm_get_pm10;
            break;
        case PM25:
            def->collection_time = MEASUREMENTS__COLLECT_TIME__HPM__MS;
            def->init_cb = hpm_init;
            def->get_cb = hpm_get_pm25;
            break;
        case MODBUS:
            def->collection_time = modbus_measurements_collection_time();
            def->init_cb = modbus_measurements_init;
            def->get_cb = modbus_measurements_get;
            break;
        case CURRENT_CLAMP:
            def->collection_time = adcs_collection_time();
            def->init_cb = adcs_begin;
            def->get_cb = adcs_get_cc;
            break;
        case W1_PROBE:
            def->collection_time = w1_collection_time();
            def->init_cb = w1_measurement_init;
            def->get_cb = w1_measurement_collect;
            break;
    }
}


void measurements_print(void)
{
    measurement_def_t* measurement_def;
    log_out("Loaded Measurements");
    log_out("Name\tInterval\tSample Count");
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def = &measurement_arr.def[i];
        char id_start = measurement_def->base.name[0];
        if (!id_start || id_start == 0xFF)
        {
            continue;
        }
        if ( 0 /* uart nearly full */ )
        {
            ; // Do something
        }
        log_out("%s\t%"PRIu8"x5mins\t\t%"PRIu8, measurement_def->base.name, measurement_def->base.interval, measurement_def->base.samplecount);
    }
}


void measurements_print_persist(void)
{
    measurement_def_base_t* measurement_def_base;
    measurement_def_base_t* persistent_measurement_arr;
    if (!persist_get_measurements(&persistent_measurement_arr))
    {
        log_error("Cannot retrieve stored measurements.");
        return;
    }
    log_out("Stored Measurements");
    log_out("Name\tInterval\tSample Count");
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_base = &persistent_measurement_arr[i];
        char id_start = measurement_def_base->name[0];
        if (!id_start || id_start == 0xFF)
        {
            continue;
        }
        if ( 0 /* uart nearly full */ )
        {
            ; // Do something
        }
        log_out("%s\t%"PRIu8"x5mins\t\t%"PRIu8, measurement_def_base->name, measurement_def_base->interval, measurement_def_base->samplecount);
    }
}


void measurements_init(void)
{
    measurement_def_base_t* persistent_measurement_arr;
    if (!persist_get_measurements(&persistent_measurement_arr))
    {
        log_error("No persistent loaded, load defaults.");
        /* Add defaults. */
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_PM10_NAME, sizeof(temp_def.base.name));
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 5;
            temp_def.base.type = PM10;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_PM25_NAME, sizeof(temp_def.base.name));
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 5;
            temp_def.base.type = PM25;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_CURRENT_CLAMP_NAME, sizeof(temp_def.base.name));
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 25;
            temp_def.base.type = CURRENT_CLAMP;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        {
            measurement_def_t temp_def;
            strncpy(temp_def.base.name, MEASUREMENT_W1_PROBE_NAME, sizeof(temp_def.base.name));
            temp_def.base.interval = 1;
            temp_def.base.samplecount = 5;
            temp_def.base.type = W1_PROBE;
            _measurement_fixup(&temp_def);
            measurements_add(&temp_def);
        }
        measurements_save();
        return;
    }

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurement_def_base_t* def_base = &persistent_measurement_arr[n];

        char id_start = def_base->name[0];

        if (!id_start || id_start == 0xFF)
            continue;

        measurement_def_t new_def;

        new_def.base = *def_base;

        _measurement_fixup(&new_def);

        measurements_add(&new_def);
    }

    /*
     *
     */
}
