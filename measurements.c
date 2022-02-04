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
#include "htu21d.h"
#include "pulsecount.h"


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
    measurement_def_t * def;
    measurement_inf_t   inf[MEASUREMENTS_MAX_NUMBER];
    measurement_data_t  data[MEASUREMENTS_MAX_NUMBER];
} measurement_arr_t;


typedef struct
{
    uint32_t last_checked_time;
    uint32_t wait_time;
} measurement_check_time_t;


static uint32_t                 last_sent_ms                                        = 0;
static bool                     pending_send                                        = false;
static measurement_check_time_t check_time                                          = {0, 0};
static uint32_t                 interval_count                                      =  0;
static int8_t                   measurements_hex_arr[MEASUREMENTS_HEX_ARRAY_SIZE]  = {0};
static uint16_t                 measurements_hex_arr_pos                            =  0;
static measurement_arr_t        measurement_arr                                     = {0};


uint32_t transmit_interval = 5; /* in minutes, defaulting to 5 minutes */

#define INTERVAL_TRANSMIT_MS   (transmit_interval * 60 * 1000)



static bool measurements_get_measurement_def(char* name, measurement_def_t** measurement_def)
{
    if (!measurement_def)
        return false;
    *measurement_def = measurements_array_find(measurement_arr.def, name);
    return (*measurement_def != NULL);
}


static bool measurements_arr_append_i8(int8_t val)
{
    if (measurements_hex_arr_pos >= MEASUREMENTS_HEX_ARRAY_SIZE)
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


static bool measurements_arr_append_float(float val)
{
    int32_t m_val = val * 1000;
    return measurements_arr_append_i32(m_val);
}


static bool measurements_arr_append_value(value_t * value)
{
    if (!value)
        return false;
    value_compact(value);
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
        case VALUE_FLOAT  : return measurements_arr_append_float(value->f);
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
    bool single = measurement_def->samplecount == 1;

    value_t mean;
    value_t num_samples;
    if (!single)
    {
        num_samples = value_from((float)measurement_data->num_samples);
    }
    else
    {
        num_samples = value_from(measurement_data->num_samples);
    }
    if (!value_div(&mean, &measurement_data->sum, &num_samples))
    {
        log_error("Failed to average %s value.", measurement_def->name);
    }

    bool r = 0;
    r |= !measurements_arr_append(*(int32_t*)measurement_def->name);
    if (single)
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS_DATATYPE_SINGLE);
        r |= !measurements_arr_append(&mean);
    }
    else
    {
        r |= !measurements_arr_append((int8_t)MEASUREMENTS_DATATYPE_AVERAGED);
        r |= !measurements_arr_append(&mean);
        r |= !measurements_arr_append(&measurement_data->min);
        r |= !measurements_arr_append(&measurement_data->max);
    }
    return !r;
}

static unsigned _message_start_pos = 0;
static unsigned _message_prev_start_pos = 0;



static void measurements_send(void)
{
    uint16_t            num_qd = 0;
    measurement_def_t*  def;
    measurement_data_t* data;

    static bool has_printed_no_con = false;

    if (!lw_get_connected())
    {
        if (!has_printed_no_con)
        {
            measurements_debug("Not connected to send, dropping readings");
            has_printed_no_con = true;
            _message_start_pos = _message_prev_start_pos = 0;
        }
        pending_send = false;
        return;
    }

    has_printed_no_con = false;

    if (!lw_send_ready())
    {
        if (pending_send)
        {
            if (since_boot_delta(since_boot_ms, last_sent_ms) > INTERVAL_TRANSMIT_MS/4)
            {
                measurements_debug("Pending send timed out.");
                lw_reconnect();
                _message_start_pos = _message_prev_start_pos = 0;
                pending_send = false;
            }
            return;
        }
        last_sent_ms = since_boot_ms;
        pending_send = true;
        return;
    }

    memset(measurements_hex_arr, 0, MEASUREMENTS_HEX_ARRAY_SIZE);
    measurements_hex_arr_pos = 0;

    measurements_debug( "Attempting to send measurements");

    if (!measurements_arr_append((int8_t)MEASUREMENTS_PAYLOAD_VERSION))
    {
        log_error("Failed to add even version to measurement hex array.");
        pending_send = false;
        last_sent_ms = since_boot_ms;
        return;
    }

    if (_message_start_pos == MEASUREMENTS_MAX_NUMBER)
        _message_start_pos = 0;

    unsigned i = _message_start_pos;

    if (_message_start_pos)
        measurements_debug("Resumming previous measurements send.");

    for (; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        def = &measurement_arr.def[i];
        data = &measurement_arr.data[i];
        if (def->interval && (interval_count % def->interval == 0))
        {
            if (data->sum.type == VALUE_UNSET || data->num_samples == 0)
            {
                data->num_samples = 0;
                data->num_samples_init = 0;
                data->num_samples_collected = 0;
                log_error("Measurement \"%s\" requested but value not set.", def->name);
                continue;
            }
            uint16_t prev_measurements_hex_arr_pos = measurements_hex_arr_pos;
            if (!measurements_to_arr(def, data))
            {
                measurements_hex_arr_pos = prev_measurements_hex_arr_pos;
                measurements_debug("Failed to queue send of  \"%s\".", def->name);
                _message_prev_start_pos = _message_start_pos;
                _message_start_pos = i;
                break;
            }
            num_qd++;
            data->sum = VALUE_EMPTY;
            data->min = VALUE_EMPTY;
            data->max = VALUE_EMPTY;
            data->num_samples = 0;
            data->num_samples_init = 0;
            data->num_samples_collected = 0;
        }
    }

    if (i == MEASUREMENTS_MAX_NUMBER)
    {
        if (_message_start_pos)
            /* Last of fragments */
            _message_prev_start_pos = _message_start_pos;
        else
            _message_prev_start_pos = 0;
        _message_start_pos = MEASUREMENTS_MAX_NUMBER;
    }

    if (num_qd > 0)
    {
        last_sent_ms = since_boot_ms;
        lw_send(measurements_hex_arr, measurements_hex_arr_pos+1);
        if (i == MEASUREMENTS_MAX_NUMBER)
        {
            pending_send = false;
            measurements_debug("Complete send");
        }
        else
        {
            pending_send = true;
            measurements_debug("Fragment send, wait to send more.");
        }
    }
}


void on_lw_sent_ack(bool ack)
{
    if (!ack)
    {
        _message_prev_start_pos = _message_start_pos = 0;
         pending_send = false;
        return;
    }

    for (unsigned i = _message_prev_start_pos; i < _message_start_pos; i++)
    {
        measurement_inf_t* inf = &measurement_arr.inf[i];

        if (inf->acked_cb)
            inf->acked_cb();
    }

    if (pending_send)
        measurements_send();
    else
        _message_prev_start_pos = _message_start_pos = 0;
}


static void measurements_sample(void)
{
    uint32_t            sample_interval;
    value_t             new_value = VALUE_EMPTY;
    uint32_t            now = since_boot_ms;
    uint32_t            time_since_interval;

    uint32_t            time_init;
    uint32_t            time_collect;

    check_time.last_checked_time = now;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_t*  def  = &measurement_arr.def[i];
        measurement_inf_t*  inf  = &measurement_arr.inf[i];
        measurement_data_t* data = &measurement_arr.data[i];

        // Breakout if the interval is 0 or has no name
        if (def->interval == 0 || !def->name[0])
        {
            continue;
        }

        sample_interval = def->interval * INTERVAL_TRANSMIT_MS / def->samplecount;
        time_since_interval = since_boot_delta(now, last_sent_ms);

        // If it takes time to get a sample, it is begun here.
        time_init = (data->num_samples_init * sample_interval) + sample_interval/2 - inf->collection_time;
        if (time_since_interval >= time_init)
        {
            data->num_samples_init++;
            if (!inf->init_cb(def->name))
            {
                data->num_samples_collected++;
            }
        }
        else if (check_time.wait_time > (time_since_interval - time_init))
        {
            check_time.wait_time = time_since_interval - time_init;
        }

        // The sample is collected every interval/samplecount but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^
        time_collect = (data->num_samples_collected * sample_interval) + sample_interval/2;
        if (time_since_interval >= time_collect)
        {
            data->num_samples_collected++;
            new_value = VALUE_EMPTY;
            if (!inf->get_cb(def->name, &new_value))
            {
                log_error("Could not get the %s value.", def->name);
                return;
            }
            if (data->min.type == VALUE_UNSET)
            {
                data->min = new_value;
            }
            if (data->max.type == VALUE_UNSET)
            {
                data->max = new_value;
            }

            if (data->sum.type == VALUE_UNSET)
            {
                data->sum = new_value;
            }
            else
            {
                if (!value_add(&data->sum, &data->sum, &new_value))
                {
                    log_error("Failed to add %s value.", def->name);
                }
            }

            data->num_samples++;
            if (value_grt(&new_value, &data->max))
            {
                data->max = new_value;
            }
            else if (value_lst(&new_value, &data->min))
            {
                data->min = new_value;
            }
            measurements_debug( "New %s reading", def->name);
            log_debug_value(DEBUG_MEASUREMENTS, "Value :", &new_value);
            log_debug_value(DEBUG_MEASUREMENTS, "Sum :", &data->sum);
            log_debug_value(DEBUG_MEASUREMENTS, "Min :", &data->min);
            log_debug_value(DEBUG_MEASUREMENTS, "Max :", &data->max);
        }
        else if (check_time.wait_time > (time_since_interval - time_collect))
        {
            check_time.wait_time = time_since_interval - time_collect;
        }
    }
}


uint16_t measurements_num_measurements(void)
{
    uint16_t count = 0;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_t*  def  = &measurement_arr.def[i];

        // Breakout if the interval is 0 or has no name
        if (def->interval == 0 || !def->name[0])
            continue;

        count++;
    }

    return count;
}


bool measurements_add(measurement_def_t* measurement_def)
{
    measurement_def_t* measurement_def_iter;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurement_def_iter = &measurement_arr.def[i];
        if (strncmp(measurement_def_iter->name, measurement_def->name, sizeof(measurement_def_iter->name)) == 0)
        {
            log_error("Tried to add measurement with the same name: %s", measurement_def->name);
            return false;
        }
        if (!measurement_arr.def[i].name[0])
        {
            measurement_data_t measurement_data = { VALUE_EMPTY, VALUE_EMPTY, VALUE_EMPTY, 0, 0, 0};
            measurement_arr.def[i] = *measurement_def;
            measurement_arr.data[i] = measurement_data;
            return true;
        }
    }
    log_error("Could not find a space to add %s", measurement_def->name);
    return false;
}


bool measurements_del(char* name)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        if (strcmp(name, measurement_arr.def[i].name) == 0)
        {
            memset(&measurement_arr.def[i], 0, sizeof(measurement_def_t));
            memset(&measurement_arr.inf[i], 0, sizeof(measurement_inf_t));
            memset(&measurement_arr.data[i], 0, sizeof(measurement_data_t));
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
    measurement_def->interval = interval;
    return true;
}


bool     measurements_get_interval(char* name, uint8_t * interval)
{
    measurement_def_t* measurement_def = NULL;
    if (!interval || !measurements_get_measurement_def(name, &measurement_def))
    {
        return false;
    }
    *interval = measurement_def->interval;
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
    measurement_def->samplecount = samplecount;
    return true;
}


bool     measurements_get_samplecount(char* name, uint8_t * samplecount)
{
    measurement_def_t* measurement_def = NULL;
    if (!samplecount || !measurements_get_measurement_def(name, &measurement_def))
    {
        return false;
    }
    *samplecount = measurement_def->samplecount;
    return true;
}


void measurements_loop_iteration(void)
{
    uint32_t now = since_boot_ms;
    if (since_boot_delta(now, check_time.last_checked_time) > check_time.wait_time)
    {
        measurements_sample();
    }
    if (since_boot_delta(now, last_sent_ms) > INTERVAL_TRANSMIT_MS)
    {
        if (interval_count > UINT32_MAX - 1)
        {
            interval_count = 0;
        }
        interval_count++;
        measurements_send();
    }
}


static void _measurement_fixup(measurement_def_t * def, measurement_inf_t * inf)
{
    inf->acked_cb = NULL;
    switch(def->type)
    {
        case PM10:
            inf->collection_time = MEASUREMENTS_COLLECT_TIME_HPM_MS;
            inf->init_cb         = hpm_init;
            inf->get_cb          = hpm_get_pm10;
            break;
        case PM25:
            inf->collection_time = MEASUREMENTS_COLLECT_TIME_HPM_MS;
            inf->init_cb         = hpm_init;
            inf->get_cb          = hpm_get_pm25;
            break;
        case MODBUS:
            inf->collection_time = modbus_measurements_collection_time();
            inf->init_cb         = modbus_measurements_init;
            inf->get_cb          = modbus_measurements_get;
            break;
        case CURRENT_CLAMP:
            inf->collection_time = adcs_cc_collection_time();
            inf->init_cb         = adcs_cc_begin;
            inf->get_cb          = adcs_cc_get;
            break;
        case W1_PROBE:
            inf->collection_time = w1_collection_time();
            inf->init_cb         = w1_measurement_init;
            inf->get_cb          = w1_measurement_collect;
            break;
        case HTU21D_TMP:
            inf->collection_time = htu21d_measurements_collection_time();
            inf->init_cb         = htu21d_temp_measurements_init;
            inf->get_cb          = htu21d_temp_measurements_get;
            break;
        case HTU21D_HUM:
            inf->collection_time = htu21d_measurements_collection_time();
            inf->init_cb         = htu21d_humi_measurements_init;
            inf->get_cb          = htu21d_humi_measurements_get;
            break;
        case BAT_MON:
            inf->collection_time = adcs_bat_collection_time();
            inf->init_cb         = adcs_bat_begin;
            inf->get_cb          = adcs_bat_get;
            break;
        case PULSE_COUNT:
            inf->collection_time = pulsecount_collection_time();
            inf->init_cb         = pulsecount_begin;
            inf->get_cb          = pulsecount_get;
            inf->acked_cb        = pulsecount_ack;
            break;
        default:
            log_error("Unknown measurement type! : 0x%"PRIx8, def->type);
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
        char id_start = measurement_def->name[0];
        if (!id_start || id_start == 0xFF)
        {
            continue;
        }
        if ( 0 /* uart nearly full */ )
        {
            ; // Do something
        }
        log_out("%s\t%"PRIu8"x%"PRIu32"mins\t\t%"PRIu8, measurement_def->name, measurement_def->interval, transmit_interval, measurement_def->samplecount);
    }
}


void measurements_init(void)
{
    measurement_arr.def = persist_get_measurements();

    unsigned found = 0;

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurement_def_t* def = &measurement_arr.def[n];
        char id_start = def->name[0];

        if (!id_start || id_start == 0xFF)
        {
            def->name[0] = 0;
            continue;
        }

        found++;
    }

    if (!found)
    {
        log_error("No persistent loaded, load defaults.");
        measurements_add_defaults(measurement_arr.def);
    }
    else measurements_debug("Loading measurements.");

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurement_def_t* def = &measurement_arr.def[n];

        if (!def->name[0])
            continue;

        _measurement_fixup(def, &measurement_arr.inf[n]);
    }

    if (!found)
        persist_commit();

    if (persist_get_mins_interval(&transmit_interval))
    {
        if (!transmit_interval)
            transmit_interval = 5;

        measurements_debug("Loading interval of %"PRIu32" minutes", transmit_interval);
    }
    else
    {
        log_error("Could not load measurement interval.");
        transmit_interval = 5;
    }
}
