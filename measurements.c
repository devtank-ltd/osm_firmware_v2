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
#include "common.h"
#include "persist_config.h"
#include "modbus_measurements.h"
#include "one_wire_driver.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "veml7700.h"


#define MEASUREMENTS_DEFAULT_COLLECTION_TIME  (uint32_t)1000


typedef enum
{
    MEASUREMENT_UNKNOWN = 0,
    MEASUREMENT_STATE_IDLE,
    MEASUREMENT_STATE_INITED,
} measurements_state_t;


typedef struct
{
    value_t                 sum;
    value_t                 max;
    value_t                 min;
    uint8_t                 num_samples;
    uint8_t                 num_samples_init;
    uint8_t                 num_samples_collected;
    measurements_state_t    state;
    uint32_t                collection_time_cache;
} measurements_data_t;


typedef struct
{
    measurements_def_t * def;
    measurements_inf_t   inf[MEASUREMENTS_MAX_NUMBER];
    measurements_data_t  data[MEASUREMENTS_MAX_NUMBER];
} measurements_arr_t;


typedef struct
{
    uint32_t last_checked_time;
    uint32_t wait_time;
} measurements_check_time_t;


static uint32_t                     _last_sent_ms                                        = 0;
static bool                         _pending_send                                        = false;
static measurements_check_time_t    _check_time                                          = {0, 0};
static uint32_t                     _interval_count                                      =  0;
static int8_t                       _measurements_hex_arr[MEASUREMENTS_HEX_ARRAY_SIZE]   = {0};
static uint16_t                     _measurements_hex_arr_pos                            =  0;
static measurements_arr_t           _measurements_arr                                     = {0};


uint32_t transmit_interval = 5; /* in minutes, defaulting to 5 minutes */

#define INTERVAL_TRANSMIT_MS   (transmit_interval * 60 * 1000)



static bool _measurements_get_measurements_def(char* name, measurements_def_t** measurements_def)
{
    if (!measurements_def)
        return false;
    *measurements_def = measurements_array_find(_measurements_arr.def, name);
    return (*measurements_def != NULL);
}


static bool _measurements_arr_append_i8(int8_t val)
{
    if (_measurements_hex_arr_pos >= MEASUREMENTS_HEX_ARRAY_SIZE)
    {
        log_error("Measurement array is full.");
        return false;
    }
    _measurements_hex_arr[_measurements_hex_arr_pos++] = val;
    return true;
}


static bool _measurements_arr_append_i16(int16_t val)
{
    return _measurements_arr_append_i8(val & 0xFF) &&
           _measurements_arr_append_i8((val >> 8) & 0xFF);
}


static bool _measurements_arr_append_i32(int32_t val)
{
    return _measurements_arr_append_i16(val & 0xFFFF) &&
           _measurements_arr_append_i16((val >> 16) & 0xFFFF);
}


static bool _measurements_arr_append_i64(int64_t val)
{
    return _measurements_arr_append_i32(val & 0xFFFFFFFF) &&
           _measurements_arr_append_i32((val >> 32) & 0xFFFFFFFF);
}


static bool _measurements_arr_append_float(float val)
{
    int32_t m_val = val * 1000;
    return _measurements_arr_append_i32(m_val);
}


static bool _measurements_arr_append_value(value_t * value)
{
    if (!value)
        return false;
    value_compact(value);
    if (!_measurements_arr_append_i8(value->type))
        return false;
    switch(value->type)
    {
        case VALUE_UINT8  : return _measurements_arr_append_i8(value->i8);
        case VALUE_INT8   : return _measurements_arr_append_i8(value->i8);
        case VALUE_UINT16 : return _measurements_arr_append_i16(value->i16);
        case VALUE_INT16  : return _measurements_arr_append_i16(value->i16);
        case VALUE_UINT32 : return _measurements_arr_append_i32(value->i32);
        case VALUE_INT32  : return _measurements_arr_append_i32(value->i32);
        case VALUE_UINT64 : return _measurements_arr_append_i64(value->i64);
        case VALUE_INT64  : return _measurements_arr_append_i64(value->i64);
        case VALUE_FLOAT  : return _measurements_arr_append_float(value->f);
        default: break;
    }
    return false;
}

#define _measurements_arr_append(_b_) _Generic((_b_),                            \
                                    signed char: _measurements_arr_append_i8,    \
                                    short int: _measurements_arr_append_i16,     \
                                    long int: _measurements_arr_append_i32,      \
                                    long long int: _measurements_arr_append_i64, \
                                    value_t * : _measurements_arr_append_value)(_b_)


static bool _measurements_to_arr(measurements_def_t* measurements_def, measurements_data_t* measurements_data)
{
    bool single = measurements_def->samplecount == 1;

    value_t mean;
    value_t num_samples;
    if (!single)
    {
        num_samples = value_from((float)measurements_data->num_samples);
    }
    else
    {
        num_samples = value_from(measurements_data->num_samples);
    }
    if (!value_div(&mean, &measurements_data->sum, &num_samples))
    {
        log_error("Failed to average %s value.", measurements_def->name);
    }

    bool r = 0;
    r |= !_measurements_arr_append(*(int32_t*)measurements_def->name);
    if (single)
    {
        r |= !_measurements_arr_append((int8_t)MEASUREMENTS_DATATYPE_SINGLE);
        r |= !_measurements_arr_append(&mean);
    }
    else
    {
        r |= !_measurements_arr_append((int8_t)MEASUREMENTS_DATATYPE_AVERAGED);
        r |= !_measurements_arr_append(&mean);
        r |= !_measurements_arr_append(&measurements_data->min);
        r |= !_measurements_arr_append(&measurements_data->max);
    }
    return !r;
}

static unsigned _message_start_pos = 0;
static unsigned _message_prev_start_pos = 0;



static void measurements_send(void)
{
    uint16_t            num_qd = 0;
    measurements_def_t*  def;
    measurements_data_t* data;

    static bool has_printed_no_con = false;

    if (!lw_get_connected())
    {
        if (!has_printed_no_con)
        {
            measurements_debug("Not connected to send, dropping readings");
            has_printed_no_con = true;
            _message_start_pos = _message_prev_start_pos = 0;
        }
        _pending_send = false;
        return;
    }

    has_printed_no_con = false;

    if (!lw_send_ready())
    {
        if (_pending_send)
        {
            if (since_boot_delta(get_since_boot_ms(), _last_sent_ms) > INTERVAL_TRANSMIT_MS/4)
            {
                measurements_debug("Pending send timed out.");
                lw_reset();
                _message_start_pos = _message_prev_start_pos = 0;
                _pending_send = false;
            }
            return;
        }
        _last_sent_ms = get_since_boot_ms();
        _pending_send = true;
        return;
    }

    memset(_measurements_hex_arr, 0, MEASUREMENTS_HEX_ARRAY_SIZE);
    _measurements_hex_arr_pos = 0;

    measurements_debug( "Attempting to send measurements");

    if (!_measurements_arr_append((int8_t)MEASUREMENTS_PAYLOAD_VERSION))
    {
        log_error("Failed to add even version to measurements hex array.");
        _pending_send = false;
        _last_sent_ms = get_since_boot_ms();
        return;
    }

    if (_message_start_pos == MEASUREMENTS_MAX_NUMBER)
        _message_start_pos = 0;

    unsigned i = _message_start_pos;

    if (_message_start_pos)
        measurements_debug("Resuming previous measurements send.");

    for (; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        def = &_measurements_arr.def[i];
        data = &_measurements_arr.data[i];
        if (def->interval && (_interval_count % def->interval == 0))
        {
            if (data->sum.type == VALUE_UNSET || data->num_samples == 0)
            {
                data->num_samples = 0;
                data->num_samples_init = 0;
                data->num_samples_collected = 0;
                log_error("Measurement \"%s\" requested but value not set.", def->name);
                continue;
            }
            uint16_t prev_measurements_hex_arr_pos = _measurements_hex_arr_pos;
            if (_measurements_hex_arr_pos >= (lw_packet_max_size / 2) ||
                !_measurements_to_arr(def, data))
            {
                _measurements_hex_arr_pos = prev_measurements_hex_arr_pos;
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
        _last_sent_ms = get_since_boot_ms();
        lw_send(_measurements_hex_arr, _measurements_hex_arr_pos+1);
        if (i == MEASUREMENTS_MAX_NUMBER)
        {
            _pending_send = false;
            measurements_debug("Complete send");
        }
        else
        {
            _pending_send = true;
            measurements_debug("Fragment send, wait to send more.");
        }
    }
}


void on_lw_sent_ack(bool ack)
{
    if (!ack)
    {
        _message_prev_start_pos = _message_start_pos = 0;
         _pending_send = false;
        return;
    }

    for (unsigned i = _message_prev_start_pos; i < _message_start_pos; i++)
    {
        measurements_inf_t* inf = &_measurements_arr.inf[i];

        if (inf->acked_cb)
            inf->acked_cb();
    }

    if (_pending_send)
        measurements_send();
    else
        _message_prev_start_pos = _message_start_pos = 0;
}


static bool _measurements_def_is_active(measurements_def_t* def)
{
    return !(def->interval == 0 || def->samplecount == 0 || !def->name[0]);
}


static uint32_t _measurements_get_collection_time(measurements_def_t* def, measurements_inf_t* inf)
{
    if (!inf->collection_time_cb)
    {
        measurements_debug("%s has no collection time iteration, using default of %"PRIu32" ms.", def->name, MEASUREMENTS_DEFAULT_COLLECTION_TIME);
        return MEASUREMENTS_DEFAULT_COLLECTION_TIME;
    }
    uint32_t collection_time;
    measurements_sensor_state_t resp = inf->collection_time_cb(def->name, &collection_time);
    switch(resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            break;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("Encountered an error retrieving collection time, using default.");
            return MEASUREMENTS_DEFAULT_COLLECTION_TIME;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            measurements_debug("Sensor is busy, using default.");
            return MEASUREMENTS_DEFAULT_COLLECTION_TIME;
    }
    return collection_time;
}


static void _measurements_sample_init_iteration(measurements_def_t* def, measurements_inf_t* inf, measurements_data_t* data)
{
    if (!inf->init_cb)
    {
        // Init functions are optional
        data->state = MEASUREMENT_STATE_INITED;
        measurements_debug("%s has no init function (optional).", def->name);
        return;
    }
    measurements_sensor_state_t resp = inf->init_cb(def->name);
    switch(resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            measurements_debug("%s successfully init'd.", def->name);
            data->state = MEASUREMENT_STATE_INITED;
            data->num_samples_init++;
            break;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s could not init, will not collect.", def->name);
            data->state = MEASUREMENT_STATE_IDLE;
            data->num_samples_init++;
            data->num_samples_collected++;
            break;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            // Sensor was busy, will retry.
            _check_time.wait_time = 0;
            break;
    }
}


static void _measurements_sample_iteration_iteration(measurements_def_t* def, measurements_inf_t* inf, measurements_data_t* data)
{
    if (!inf->iteration_cb)
    {
        // Iteration callbacks are optional
        return;
    }
    measurements_sensor_state_t resp = inf->iteration_cb(def->name);
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            return;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s errored on iterate, will not collect.", def->name);
            data->state = MEASUREMENT_STATE_IDLE;
            data->num_samples_init++;
            data->num_samples_collected++;
            return;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            return;
    }
}


static bool _measurements_sample_get_iteration(measurements_def_t* def, measurements_inf_t* inf, measurements_data_t* data)
{
    if (!inf->get_cb)
    {
        // Get function is non-optional
        data->state = MEASUREMENT_STATE_IDLE;
        measurements_debug("%s has no collect function.", def->name);
        return false;
    }
    value_t new_value = VALUE_EMPTY;
    measurements_sensor_state_t resp = inf->get_cb(def->name, &new_value);
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            measurements_debug("%s successfully collect'd.", def->name);
            data->state = MEASUREMENT_STATE_IDLE;
            data->num_samples_collected++;
            break;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s could not collect.", def->name);
            data->state = MEASUREMENT_STATE_IDLE;
            data->num_samples_collected++;
            return false;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            // Sensor was busy, will retry.
            _check_time.wait_time = 0;
            return false;
    }
    log_debug_value(DEBUG_MEASUREMENTS, "Value :", &new_value);

    if (data->num_samples == 0)
    {
        // If first measurements
        data->num_samples++;
        data->min = new_value;
        data->max = new_value;
        data->sum = new_value;
        return true;
    }

    if (!value_add(&data->sum, &data->sum, &new_value))
    {
        // If this fails, the number of samples shouldn't increase nor the max/min.
        log_error("Failed to add %s value.", def->name);
        return false;
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
    return true;
}


static void _measurements_sample(void)
{
    uint32_t            sample_interval;
    uint32_t            now = get_since_boot_ms();
    uint32_t            time_since_interval;

    uint32_t            time_init_boundary;
    uint32_t            time_init;
    uint32_t            time_collect;

    _check_time.last_checked_time = now;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];
        measurements_inf_t*  inf  = &_measurements_arr.inf[i];
        measurements_data_t* data = &_measurements_arr.data[i];

        // Breakout if the interval or samplecount is 0 or has no name
        if (!_measurements_def_is_active(def))
        {
            continue;
        }

        sample_interval = def->interval * INTERVAL_TRANSMIT_MS / def->samplecount;
        time_since_interval = since_boot_delta(now, _last_sent_ms);

        time_init_boundary = (data->num_samples_init * sample_interval) + sample_interval/2;
        if (time_init_boundary < data->collection_time_cache)
        {
            // Assert that no negative rollover could happen for long collection times. Just do it immediately.
            data->collection_time_cache = time_init_boundary;
        }
        time_init       = time_init_boundary - data->collection_time_cache;
        time_collect    = (data->num_samples_collected  * sample_interval) + sample_interval/2;
        // If it takes time to get a sample, it is begun here.
        if (time_since_interval >= time_init)
        {
            _measurements_sample_init_iteration(def, inf, data);
        }
        else if (_check_time.wait_time > (time_since_interval - time_init))
        {
            _check_time.wait_time = time_since_interval - time_init;
        }

        // The sample is collected every interval/samplecount but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^

        if (time_since_interval >= time_collect)
        {
            if (_measurements_sample_get_iteration(def, inf, data))
            {
                log_debug_value(DEBUG_MEASUREMENTS, "Sum :", &data->sum);
                log_debug_value(DEBUG_MEASUREMENTS, "Min :", &data->min);
                log_debug_value(DEBUG_MEASUREMENTS, "Max :", &data->max);
                data->collection_time_cache = _measurements_get_collection_time(def, inf);
            }
        }
        else if (_check_time.wait_time > (time_since_interval - time_collect))
        {
            _check_time.wait_time = time_since_interval - time_collect;
        }
    }
}


uint16_t measurements_num_measurements(void)
{
    uint16_t count = 0;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];

        // Breakout if the interval is 0 or has no name
        if (def->interval == 0 || !def->name[0])
            continue;

        count++;
    }

    return count;
}


static void _measurements_fixup(measurements_def_t * def, measurements_inf_t * inf, measurements_data_t* data)
{
    // Optional callbacks, get and collection time are not optional.
    inf->init_cb        = NULL;
    inf->iteration_cb   = NULL;
    inf->acked_cb       = NULL;
    switch(def->type)
    {
        case PM10:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm10;
            break;
        case PM25:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm25;
            break;
        case MODBUS:
            inf->collection_time_cb = modbus_measurements_collection_time;
            inf->init_cb            = modbus_measurements_init;
            inf->get_cb             = modbus_measurements_get;
            break;
        case CURRENT_CLAMP:
            inf->collection_time_cb = adcs_cc_collection_time;
            inf->init_cb            = adcs_cc_begin;
            inf->get_cb             = adcs_cc_get;
            break;
        case W1_PROBE:
            inf->collection_time_cb = w1_collection_time;
            inf->init_cb            = w1_measurements_init;
            inf->get_cb             = w1_measurements_collect;
            break;
        case HTU21D_TMP:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_temp_measurements_init;
            inf->get_cb             = htu21d_temp_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            break;
        case HTU21D_HUM:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_humi_measurements_init;
            inf->get_cb             = htu21d_humi_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            break;
        case BAT_MON:
            inf->collection_time_cb = adcs_bat_collection_time;
            inf->init_cb            = adcs_bat_begin;
            inf->get_cb             = adcs_bat_get;
            break;
        case PULSE_COUNT:
            inf->collection_time_cb = pulsecount_collection_time;
            inf->init_cb            = pulsecount_begin;
            inf->get_cb             = pulsecount_get;
            inf->acked_cb           = pulsecount_ack;
            break;
        case LIGHT:
            inf->collection_time_cb = veml7700_measurements_collection_time;
            inf->init_cb            = veml7700_light_measurements_init;
            inf->get_cb             = veml7700_light_measurements_get;
            inf->iteration_cb       = veml7700_iteration;
            break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
    }
    data->collection_time_cache = _measurements_get_collection_time(def, inf);
}

static void _measurements_derive_cc_phase(void)
{
    unsigned len = 0;
    uint8_t channels[ADC_COUNT] = {0};
    measurements_def_t* def;
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_1_NAME, &def) &&
        _measurements_def_is_active(def) )
    {
        channels[len++] = ADC1_CHANNEL_CURRENT_CLAMP_1;
    }
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_2_NAME, &def) &&
        _measurements_def_is_active(def) )
    {
        channels[len++] = ADC1_CHANNEL_CURRENT_CLAMP_2;
    }
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_3_NAME, &def) &&
        _measurements_def_is_active(def) )
    {
        channels[len++] = ADC1_CHANNEL_CURRENT_CLAMP_3;
    }
    adcs_cc_set_channels_active(channels, len);
}


bool measurements_add(measurements_def_t* measurements_def)
{
    measurements_def_t* def;
    measurements_inf_t* inf;
    measurements_data_t* data;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        def = &_measurements_arr.def[i];
        inf = &_measurements_arr.inf[i];
        data = &_measurements_arr.data[i];
        if (strncmp(def->name, measurements_def->name, sizeof(def->name)) == 0)
        {
            log_error("Tried to add measurements with the same name: %s", measurements_def->name);
            return false;
        }
        if (!def->name[0])
        {
            measurements_data_t data_empty = { VALUE_EMPTY, VALUE_EMPTY, VALUE_EMPTY, 0, 0, 0, MEASUREMENT_STATE_IDLE, 0};
            memcpy(def, measurements_def, sizeof(measurements_def_t));
            _measurements_fixup(def, inf, data);
            memcpy(data, &data_empty, sizeof(measurements_data_t));
            _measurements_derive_cc_phase();
            return true;
        }
    }
    log_error("Could not find a space to add %s", measurements_def->name);
    return false;
}


bool measurements_del(char* name)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        if (strcmp(name, _measurements_arr.def[i].name) == 0)
        {
            memset(&_measurements_arr.def[i], 0, sizeof(measurements_def_t));
            memset(&_measurements_arr.inf[i], 0, sizeof(measurements_inf_t));
            memset(&_measurements_arr.data[i], 0, sizeof(measurements_data_t));
            _measurements_derive_cc_phase();
            return true;
        }
    }
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurements_def_t* measurements_def = NULL;
    if (!_measurements_get_measurements_def(name, &measurements_def))
    {
        return false;
    }
    measurements_def->interval = interval;
    _measurements_derive_cc_phase();
    return true;
}


bool measurements_get_interval(char* name, uint8_t * interval)
{
    measurements_def_t* measurements_def = NULL;
    if (!interval || !_measurements_get_measurements_def(name, &measurements_def))
    {
        return false;
    }
    *interval = measurements_def->interval;
    return true;
}


bool measurements_set_samplecount(char* name, uint8_t samplecount)
{
    if (samplecount == 0)
    {
        log_error("Cannot set the samplecount to 0.");
        return false;
    }
    measurements_def_t* measurements_def = NULL;
    if (!_measurements_get_measurements_def(name, &measurements_def))
    {
        return false;
    }
    measurements_def->samplecount = samplecount;
    _measurements_derive_cc_phase();
    return true;
}


bool     measurements_get_samplecount(char* name, uint8_t * samplecount)
{
    measurements_def_t* measurements_def = NULL;
    if (!samplecount || !_measurements_get_measurements_def(name, &measurements_def))
    {
        return false;
    }
    *samplecount = measurements_def->samplecount;
    return true;
}


static void _measurements_iterate_callbacks(void)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        if (_measurements_arr.data[i].state == MEASUREMENT_STATE_INITED)
        {
            _measurements_sample_iteration_iteration(&_measurements_arr.def[i], &_measurements_arr.inf[i], &_measurements_arr.data[i]);
        }
    }
}


void measurements_loop_iteration(void)
{
    uint32_t now = get_since_boot_ms();
    if (since_boot_delta(now, _check_time.last_checked_time) > _check_time.wait_time)
    {
        _measurements_sample();
    }
    if (since_boot_delta(now, _last_sent_ms) > INTERVAL_TRANSMIT_MS)
    {
        if (_interval_count > UINT32_MAX - 1)
        {
            _interval_count = 0;
        }
        _interval_count++;
        measurements_send();
    }
    _measurements_iterate_callbacks();
}


void measurements_print(void)
{
    measurements_def_t* measurements_def;
    log_out("Loaded Measurements");
    log_out("Name\tInterval\tSample Count");
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def = &_measurements_arr.def[i];
        char id_start = measurements_def->name[0];
        if (!id_start || id_start == 0xFF)
        {
            continue;
        }
        if ( 0 /* uart nearly full */ )
        {
            ; // Do something
        }
        log_out("%s\t%"PRIu8"x%"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval, measurements_def->samplecount);
    }
}


void measurements_init(void)
{
    _measurements_arr.def = persist_get_measurements();

    unsigned found = 0;

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurements_def_t* def = &_measurements_arr.def[n];
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
        measurements_add_defaults(_measurements_arr.def);
    }
    else measurements_debug("Loading measurements.");

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurements_def_t* def = &_measurements_arr.def[n];

        if (!def->name[0])
            continue;

        _measurements_fixup(def, &_measurements_arr.inf[n], &_measurements_arr.data[n]);
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
        log_error("Could not load measurements interval.");
        transmit_interval = 5;
    }
    _measurements_derive_cc_phase();
}
