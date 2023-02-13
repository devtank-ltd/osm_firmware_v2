#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "measurements.h"
#include "comms.h"
#include "log.h"
#include "config.h"
#include "common.h"
#include "persist_config.h"
#include "sleep.h"
#include "uart_rings.h"
#include "bat.h"
#include "platform.h"
#include "platform_model.h"
#include "protocol.h"


#define MEASUREMENTS_DEFAULT_COLLECTION_TIME    (uint32_t)1000


typedef struct
{
    measurements_def_t * def;
    measurements_data_t  data[MEASUREMENTS_MAX_NUMBER];
} measurements_arr_t;


typedef struct
{
    uint32_t last_checked_time;
    uint32_t wait_time;
} measurements_check_time_t;


typedef struct
{
    measurements_def_t* def;
    measurements_data_t* data;
    measurements_inf_t* inf;
} measurements_info_t;


static uint32_t                     _last_sent_ms                                        = 0;
static bool                         _pending_send                                        = false;
static measurements_check_time_t    _check_time                                          = {0, 0};
static uint32_t                     _interval_count                                      =  0;
static int8_t                       _measurements_hex_arr[MEASUREMENTS_HEX_ARRAY_SIZE]   = {0};
static measurements_arr_t           _measurements_arr                                    = {0};
static bool                         _measurements_debug_mode                             = false;

bool                                 measurements_enabled                                = true;

static measurements_power_mode_t    _measurements_power_mode                             = MEASUREMENTS_POWER_MODE_AUTO;

static unsigned _measurements_chunk_start_pos = 0;
static unsigned _measurements_chunk_prev_start_pos = 0;


uint32_t transmit_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL; /* in minutes, defaulting to 15 minutes */

#define INTERVAL_TRANSMIT_MS   (transmit_interval * 60)


bool measurements_get_measurements_def(char* name, measurements_def_t ** measurements_def, measurements_data_t ** measurements_data)
{
    if (!name || strlen(name) > MEASURE_NAME_LEN || !name[0])
        return false;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t * def   = &_measurements_arr.def[i];
        measurements_data_t * data = &_measurements_arr.data[i];
        if (strncmp(def->name, name, MEASURE_NAME_LEN) == 0)
        {
            if (measurements_def)
                *measurements_def = def;
            if (measurements_data)
                *measurements_data = data;
            return true;
        }
    }

    return false;
}


static bool _measurements_send_start(void)
{
    unsigned mtu_size = (comms_get_mtu() / 2);
    unsigned buf_size = ARRAY_SIZE(_measurements_hex_arr);
    unsigned size = mtu_size < buf_size ? mtu_size : buf_size;
    memset(_measurements_hex_arr, 0, MEASUREMENTS_HEX_ARRAY_SIZE);
    if (!protocol_init(_measurements_hex_arr, size))
    {
        log_error("Failed to add even version to measurements hex array.");
        _pending_send = false;
        _last_sent_ms = get_since_boot_ms();
        return false;
    }

    return true;
}


bool measurements_send_test(char * name)
{
    if (!comms_get_connected() || !comms_send_ready())
    {
        measurements_debug("LW not ready.");
        return false;
    }

    if (!_measurements_send_start())
    {
        measurements_debug("Failed to send start.");
        return false;
    }

    measurements_reading_t v;
    measurements_value_type_t v_type;

    if (!measurements_get_reading(name, &v, &v_type))
    {
        measurements_debug("Failed to get reading of %s.", name);
        return false;
    }

    measurements_def_t* def;
    measurements_data_t* data;
    if (!measurements_get_measurements_def(name, &def, &data))
    {
        measurements_debug("Unable to get measurements definition.");
        return false;
    }

    bool r = protocol_append_measurement(def, data);

    if (r)
    {
        measurements_debug("Sending test array.");
        comms_send(_measurements_hex_arr, protocol_get_length());
    }
    else measurements_debug("Failed to add to array.");

    return r;
}


static void _measurements_send(void)
{
    uint16_t            num_qd = 0;

    static bool has_printed_no_con = false;

    if (!comms_get_connected())
    {
        if (!has_printed_no_con)
        {
            measurements_debug("Not connected to send, dropping readings");
            has_printed_no_con = true;
            _measurements_chunk_start_pos = _measurements_chunk_prev_start_pos = 0;
        }
        _pending_send = false;
        if (_measurements_debug_mode)
            _last_sent_ms = get_since_boot_ms();
        else
            return;
    }

    has_printed_no_con = false;

    if (!comms_send_ready() && !_measurements_debug_mode)
    {
        if (comms_send_allowed())
        {
            // Tried to send but not allowed (receiving FW?)
            return;
        }
        if (_pending_send)
        {
            if (since_boot_delta(get_since_boot_ms(), _last_sent_ms) > INTERVAL_TRANSMIT_MS/4)
            {
                measurements_debug("Pending send timed out.");
                comms_reset();
                _measurements_chunk_start_pos = _measurements_chunk_prev_start_pos = 0;
                _pending_send = false;
            }
            return;
        }
        _last_sent_ms = get_since_boot_ms();
        _pending_send = true;
        return;
    }

    if (!_measurements_send_start())
        return;

    if (_measurements_chunk_start_pos == MEASUREMENTS_MAX_NUMBER)
        _measurements_chunk_start_pos = 0;

    unsigned i = _measurements_chunk_start_pos;

    if (_measurements_chunk_start_pos)
        measurements_debug("Resuming previous measurements send.");

    for (; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];
        measurements_data_t* data = &_measurements_arr.data[i];
        if (def->interval && (_interval_count % def->interval == 0))
        {
            if (data->num_samples == 0)
            {
                data->num_samples_init = 0;
                data->num_samples_collected = 0;
                log_error("Measurement \"%s\" requested but value not set.", def->name);
                continue;
            }
            if (!protocol_append_measurement(def, data))
            {
                measurements_debug("Failed to queue send of  \"%s\".", def->name);
                _measurements_chunk_prev_start_pos = _measurements_chunk_start_pos;
                _measurements_chunk_start_pos = i;
                break;
            }
            num_qd++;
            memset(&data->value, 0, sizeof(measurements_value_t));
            data->num_samples = 0;
            data->num_samples_init = 0;
            data->num_samples_collected = 0;
        }
    }

    if (i == MEASUREMENTS_MAX_NUMBER)
    {
        if (_measurements_chunk_start_pos)
            /* Last of fragments */
            _measurements_chunk_prev_start_pos = _measurements_chunk_start_pos;
        else
            _measurements_chunk_prev_start_pos = 0;
        _measurements_chunk_start_pos = MEASUREMENTS_MAX_NUMBER;
    }

    if (!_pending_send)
        _last_sent_ms = get_since_boot_ms();
    if (num_qd > 0)
    {
        if (_measurements_debug_mode)
        {
            for (unsigned j = 0; j < protocol_get_length(); j++)
                measurements_debug("Packet %u = 0x%"PRIx8, j, _measurements_hex_arr[j]);
        }
        else
            comms_send(_measurements_hex_arr, protocol_get_length());
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


static uint32_t _measurements_get_collection_time(measurements_def_t* def, measurements_inf_t* inf)
{
    if (!def || !inf)
        // Doesnt exist
        return 0;
    if (!inf->collection_time_cb)
    {
        if (!inf->init_cb)
        {
            // If no init is required, neither is a collection time cb.
            return 0;
        }
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


void on_comms_sent_ack(bool ack)
{
    if (!ack)
    {
        _measurements_chunk_prev_start_pos = _measurements_chunk_start_pos = 0;
         _pending_send = false;
        return;
    }

    for (unsigned i = _measurements_chunk_prev_start_pos; i < _measurements_chunk_start_pos; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];
        if (!def->name[0])
            continue;
        measurements_inf_t inf;
        if (!model_measurements_get_inf(def, NULL, &inf))
            continue;

        if (inf.acked_cb)
            inf.acked_cb(def->name);
    }

    if (_pending_send)
        _measurements_send();
    else
        _measurements_chunk_prev_start_pos = _measurements_chunk_start_pos = 0;
}


static bool _measurements_def_is_active(measurements_def_t* def)
{
    return !(def->interval == 0 || def->samplecount == 0 || !def->name[0]);
}


static void _measurements_sample_init_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        return;
    }
    if (data->is_collecting)
    {
        measurements_debug("Measurement %s already collecting.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        return;
    }
    if (!inf.init_cb)
    {
        // Init functions are optional
        data->num_samples_init++;
        data->is_collecting = 1;
        measurements_debug("%s has no init function (optional).", def->name);
        return;
    }
    measurements_sensor_state_t resp = inf.init_cb(def->name, false);
    switch(resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            measurements_debug("%s successfully init'd.", def->name);
            data->num_samples_init++;
            data->is_collecting = 1;
            break;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s could not init, will not collect.", def->name);
            data->num_samples_init++;
            data->num_samples_collected++;
            break;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            // Sensor was busy, will retry.
            _check_time.wait_time = 0;
            break;
    }
}


static bool _measurements_sample_iteration_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        return false;
    }
    if (!inf.iteration_cb)
    {
        // Iteration callbacks are optional
        return false;
    }
    measurements_sensor_state_t resp = inf.iteration_cb(def->name);
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            return true;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s errored on iterate, will not collect.", def->name);
            data->num_samples_init++;
            data->num_samples_collected++;
            return false;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            return true;
    }
    return false;
}


static bool _measurements_sample_get_resp(measurements_def_t* def, measurements_data_t* data, measurements_inf_t* inf, measurements_reading_t* new_value)
{
    if (!def || !data || !inf || !new_value)
    {
        measurements_debug("Handed a NULL pointer.");
        return false;
    }
    /* Each function should check if this has been initialised */
    measurements_sensor_state_t resp = inf->get_cb(def->name, new_value);
    data->is_collecting = 0;
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            measurements_debug("%s successfully collect'd.", def->name);
            data->num_samples_collected++;
            break;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("%s could not collect.", def->name);
            data->num_samples_collected++;
            return false;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            // Sensor was busy, will retry.
            _check_time.wait_time = 0;
            return false;
    }
    return true;
}


static bool _measurements_sample_get_i64_iteration(measurements_def_t* def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !data || !inf)
    {
        measurements_debug("Handed a NULL pointer.");
        return false;
    }
    measurements_reading_t new_value;
    if (!_measurements_sample_get_resp(def, data, inf, &new_value))
    {
        return false;
    }
    measurements_debug("Value : %"PRIi64, new_value.v_i64);

    if (data->num_samples == 0)
    {
        // If first measurements
        data->num_samples++;
        data->value.value_64.min = new_value.v_i64;
        data->value.value_64.max = new_value.v_i64;
        data->value.value_64.sum = new_value.v_i64;
        goto good_exit;
    }

    data->value.value_64.sum += new_value.v_i64;

    data->num_samples++;

    if (new_value.v_i64 > data->value.value_64.max)
        data->value.value_64.max = new_value.v_i64;

    else if (new_value.v_i64 < data->value.value_64.min)
        data->value.value_64.min = new_value.v_i64;

good_exit:
    measurements_debug("Sum : %"PRIi64, data->value.value_64.sum);
    measurements_debug("Min : %"PRIi64, data->value.value_64.min);
    measurements_debug("Max : %"PRIi64, data->value.value_64.max);

    return true;
}


static bool _measurements_sample_get_str_iteration(measurements_def_t* def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !data || !inf)
    {
        measurements_debug("Handed a NULL pointer.");
        return false;
    }
    measurements_reading_t new_value;
    if (!_measurements_sample_get_resp(def, data, inf, &new_value))
    {
        measurements_debug("Sample returned false.");
        return false;
    }
    uint8_t new_len = strnlen(new_value.v_str, MEASUREMENTS_VALUE_STR_LEN - 1);
    strncpy(data->value.value_s.str, new_value.v_str, new_len);
    data->value.value_s.str[new_len] = 0;
    measurements_debug("Value : %s", data->value.value_s.str);
    data->num_samples++;
    return true;
}


static bool _measurements_sample_get_float_iteration(measurements_def_t* def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !data || !inf)
    {
        measurements_debug("Handed a NULL pointer.");
        return false;
    }
    measurements_reading_t new_value;
    if (!_measurements_sample_get_resp(def, data, inf, &new_value))
    {
        return false;
    }
    measurements_debug("Value : %"PRIi32".%03"PRIu32, new_value.v_f32/1000, (uint32_t)abs(new_value.v_f32)%1000);

    if (data->num_samples == 0)
    {
        // If first measurements
        data->num_samples++;
        data->value.value_f.min = new_value.v_f32;
        data->value.value_f.max = new_value.v_f32;
        data->value.value_f.sum = new_value.v_f32;
        goto good_exit;
    }

    data->value.value_f.sum += new_value.v_f32;

    data->num_samples++;

    if (new_value.v_f32 > data->value.value_f.max)
        data->value.value_f.max = new_value.v_f32;

    else if (new_value.v_f32 < data->value.value_f.min)
        data->value.value_f.min = new_value.v_f32;

good_exit:
    measurements_debug("Sum : %"PRIi32".%03"PRIu32, data->value.value_f.sum/1000, (uint32_t)abs(data->value.value_f.sum)%1000);
    measurements_debug("Min : %"PRIi32".%03"PRIu32, data->value.value_f.min/1000, (uint32_t)abs(data->value.value_f.min)%1000);
    measurements_debug("Max : %"PRIi32".%03"PRIu32, data->value.value_f.max/1000, (uint32_t)abs(data->value.value_f.max)%1000);

    return true;
}


static bool _measurements_sample_get_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_collected++;
        return false;
    }
    if (!inf.get_cb)
    {
        // Get function is non-optional
        data->num_samples_collected++;
        measurements_debug("%s has no collect function.", def->name);
        return false;
    }

    bool r;
    switch(data->value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            r = _measurements_sample_get_i64_iteration(def, data, &inf);
            break;
        case MEASUREMENTS_VALUE_TYPE_STR:
            r = _measurements_sample_get_str_iteration(def, data, &inf);
            break;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            r = _measurements_sample_get_float_iteration(def, data, &inf);
            break;
        default:
            measurements_debug("Unknown type '%"PRIu8"'. Don't know what to do.", data->value_type);
            return false;
    }

    data->collection_time_cache = _measurements_get_collection_time(def, &inf);
    return r;
}


static void _measurements_sample(void)
{
    uint32_t            sample_interval;
    uint32_t            now = get_since_boot_ms();
    uint32_t            time_since_interval;

    uint32_t            time_init_boundary;
    uint32_t            time_init;
    uint32_t            time_collect;

    uint32_t            wait_time;

    _check_time.last_checked_time = now;
    _check_time.wait_time = UINT32_MAX;

    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];
        measurements_data_t* data = &_measurements_arr.data[i];

        // Breakout if the interval or samplecount is 0 or has no name
        if (!_measurements_def_is_active(def))
        {
            continue;
        }

        sample_interval = def->interval * INTERVAL_TRANSMIT_MS / def->samplecount;
        time_since_interval = since_boot_delta(now, _last_sent_ms) + (_interval_count % def->interval) * INTERVAL_TRANSMIT_MS;

        time_init_boundary = (data->num_samples_init * sample_interval) + sample_interval/2;
        if (time_init_boundary < data->collection_time_cache)
        {
            // Assert that no negative rollover could happen for long collection times. Just do it immediately.
            data->collection_time_cache = time_init_boundary;
        }
        time_init       = time_init_boundary - data->collection_time_cache;
        time_collect    = (data->num_samples_collected  * sample_interval) + sample_interval/2;
        if (time_since_interval >= time_init)
        {
            if (data->num_samples_collected < data->num_samples_init)
            {
                data->num_samples_collected++;
                measurements_debug("Could not collect before next init.");
            }
            _measurements_sample_init_iteration(def, data);
            wait_time = since_boot_delta(data->collection_time_cache + time_init, time_since_interval);
        }
        else
        {
            wait_time = since_boot_delta(time_init, time_since_interval);
        }

        if (_check_time.wait_time > wait_time)
        {
            _check_time.wait_time = wait_time;
        }

        // The sample is collected every interval/samplecount but offset by 1/2.
        // ||   .   .   .   .   .   ||   .   .   .   .   .   ||
        //    ^   ^   ^   ^   ^   ^    ^   ^   ^   ^   ^   ^

        if (time_since_interval >= time_collect)
        {
            _measurements_sample_get_iteration(def, data);
            wait_time = since_boot_delta(time_collect + sample_interval, data->collection_time_cache + time_since_interval);
        }
        else
        {
            wait_time = since_boot_delta(time_collect, time_since_interval);
        }

        if (_check_time.wait_time > wait_time)
        {
            _check_time.wait_time = wait_time;
        }
    }
    if (_check_time.wait_time == UINT32_MAX)
        _check_time.wait_time = 0;
}


bool     measurements_for_each(measurements_for_each_cb_t cb, void * data)
{
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];

        if (def->interval == 0 || !def->name[0])
            continue;

        if (!cb(def, data))
            return false;
    }
    return true;
}


bool measurements_add(measurements_def_t* measurements_def)
{
    bool                    found_space = false;
    unsigned                space;
    measurements_def_t*     def;
    measurements_data_t*    data;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        def = &_measurements_arr.def[i];
        data = &_measurements_arr.data[i];
        if (!def->name[0])
        {
            if (!found_space)
            {
                found_space = true;
                space = i;
            }
            continue;
        }
        if (strncmp(def->name, measurements_def->name, sizeof(def->name)) == 0)
        {
            log_error("Tried to add measurements with the same name: %s", measurements_def->name);
            return false;
        }
    }
    if (found_space)
    {
        def = &_measurements_arr.def[space];
        data = &_measurements_arr.data[space];
        memcpy(def, measurements_def, sizeof(measurements_def_t));
        memset(data, 0, sizeof(measurements_data_t));
        {
            measurements_inf_t inf;
            if (!model_measurements_get_inf(def, data, &inf))
            {
                log_error("Could not get measurement interface.");
                return false;
            }
            data->collection_time_cache = _measurements_get_collection_time(def, &inf);
            if (inf.enable_cb)
                inf.enable_cb(def->name, def->interval > 0);
        }
        return true;
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
            measurements_def_t*  def = &_measurements_arr.def[i];
            measurements_data_t* data = &_measurements_arr.data[i];
            measurements_inf_t inf;
            if (!model_measurements_get_inf(def, data, &inf))
                return false;
            if (inf.enable_cb)
                inf.enable_cb(def->name, false);
            memset(def, 0, sizeof(measurements_def_t));
            memset(data, 0, sizeof(measurements_data_t));
            return true;
        }
    }
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurements_def_t* def = NULL;
    measurements_data_t* data;
    if (!measurements_get_measurements_def(name, &def, &data))
    {
        return false;
    }

    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
        return false;

    if (inf.enable_cb && ((interval > 0) != (def->interval > 0)))
        inf.enable_cb(name, interval > 0);

    def->interval = interval;
    return true;
}


bool measurements_get_interval(char* name, uint8_t * interval)
{
    measurements_def_t* measurements_def = NULL;
    if (!interval || !measurements_get_measurements_def(name, &measurements_def, NULL))
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
    if (!measurements_get_measurements_def(name, &measurements_def, NULL))
    {
        return false;
    }
    measurements_def->samplecount = samplecount;
    return true;
}


bool measurements_get_samplecount(char* name, uint8_t * samplecount)
{
    measurements_def_t* measurements_def = NULL;
    if (!samplecount || !measurements_get_measurements_def(name, &measurements_def, NULL))
    {
        return false;
    }
    *samplecount = measurements_def->samplecount;
    return true;
}


static uint16_t _measurements_iterate_callbacks(void)
{
    uint16_t active_count = 0;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        if (_measurements_arr.data[i].num_samples_init <= _measurements_arr.data[i].num_samples_collected)
            continue;
        _measurements_sample_iteration_iteration(&_measurements_arr.def[i], &_measurements_arr.data[i]);
        active_count++;
    }
    return active_count;
}


static void _measurements_sleep_iteration(void)
{
    static bool _measurements_print_sleep = false;

    bool on_bat;
    switch (_measurements_power_mode)
    {
        case MEASUREMENTS_POWER_MODE_AUTO:
            if (!bat_on_battery(&on_bat))
                return;
            if (!on_bat)
                return;
            break;
        case MEASUREMENTS_POWER_MODE_BATTERY:
            break;
        case MEASUREMENTS_POWER_MODE_PLUGGED:
            return;
    }

    uint32_t sleep_time;
    /* Get new now as above functions may have taken a while */
    uint32_t now = get_since_boot_ms();

    if (since_boot_delta(now, _check_time.last_checked_time) >= _check_time.wait_time)
        return;

    if (since_boot_delta(now, _last_sent_ms) >= INTERVAL_TRANSMIT_MS)
        return;

    uint32_t next_sample_time = since_boot_delta(_check_time.last_checked_time + _check_time.wait_time, now);
    uint32_t next_send_time = since_boot_delta(_last_sent_ms + INTERVAL_TRANSMIT_MS, now);
    if (next_sample_time < next_send_time)
        sleep_time = next_sample_time;
    else
        sleep_time = next_send_time;

    if (sleep_time < SLEEP_MIN_SLEEP_TIME_MS)
        /* No point sleeping for short amount of time */
        return;

    /* Make sure to wake up before required */
    sleep_time -= SLEEP_MIN_SLEEP_TIME_MS;
    if (_measurements_print_sleep)
    {
        _measurements_print_sleep = false;
    }
    if (sleep_for_ms(sleep_time))
        _measurements_print_sleep = true;
}


static bool _measurements_get_reading2(measurements_def_t* def, measurements_data_t* data, measurements_reading_t* reading, measurements_value_type_t* type);


void _measurements_check_instant_send(void)
{
    bool to_instant_send = false;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        if (_measurements_arr.data[i].instant_send)
            to_instant_send = true;
    }

    if (!to_instant_send)
        return;

    unsigned mtu_size = (comms_get_mtu() / 2);
    unsigned buf_size = ARRAY_SIZE(_measurements_hex_arr);
    unsigned size = mtu_size < buf_size ? mtu_size : buf_size;
    memset(_measurements_hex_arr, 0, MEASUREMENTS_HEX_ARRAY_SIZE);
    if (!protocol_init(_measurements_hex_arr, size))
    {
        measurements_debug("Could not initialise the hex array for the protocol.");
        return;
    }

    unsigned count = 0;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def_t* def = &_measurements_arr.def[i];
        measurements_data_t* data = &_measurements_arr.data[i];
        if (data->instant_send)
        {
            data->instant_send = 0;
            /* TODO: Add a check to ensure last sent wasn't 0 seconds ago */
            measurements_reading_t reading;
            measurements_value_type_t type;
            if (!_measurements_get_reading2(def, data, &reading, &type))
            {
                measurements_debug("Could not get measurement '%s' for instant send.", def->name);
                continue;
            }
            if (!protocol_append_instant_measurement(def, &reading, type))
            {
                measurements_debug("Could not add measurement '%s' to array.", def->name);
                break;
            }
            count++;
        }
    }
    if (!count)
    {
        measurements_debug("No measurements were added, not sending.");
        return;
    }
    comms_send(_measurements_hex_arr, protocol_get_length());
}


void measurements_loop_iteration(void)
{
    if (!measurements_enabled)
        return;

    static bool has_printed_no_con = false;

    if (!comms_get_connected() && !_measurements_debug_mode)
    {
        if (!has_printed_no_con)
        {
            measurements_debug("Not connected to send, not taking readings.");
            has_printed_no_con = true;
            _measurements_chunk_start_pos = _measurements_chunk_prev_start_pos = 0;
            _pending_send = false;
        }
        _measurements_iterate_callbacks();
        return;
    }
    uint32_t now = get_since_boot_ms();

    if (has_printed_no_con)
    {
        measurements_debug("Connected to send, starting readings.");
        _last_sent_ms = now;
        has_printed_no_con = false;
        return;
    }

    _measurements_check_instant_send();

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
        _measurements_send();
    }
    uint16_t count_active = _measurements_iterate_callbacks();
    /* If no measurements require active calls. */
    if (count_active == 0)
    {
        _measurements_sleep_iteration();
    }
}


void measurements_print(void)
{
    measurements_def_t* measurements_def;
    log_out("Loaded Measurements");
    log_out("Name\tInterval\tSample Count");
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def = &_measurements_arr.def[i];
        unsigned char id_start = measurements_def->name[0];
        if (!id_start || id_start == 0xFF)
            continue;
        if (transmit_interval % 1000)
            log_out("%s\t%"PRIu8"x%"PRIu32".%03"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval/1000, transmit_interval%1000, measurements_def->samplecount);
        else
            log_out("%s\t%"PRIu8"x%"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval/1000, measurements_def->samplecount);
    }
}


static void _measurements_replace_name_if_legacy(char* dest_name, char* old_name, char* new_name)
{
    if (strncmp(dest_name, old_name, MEASURE_NAME_LEN) == 0)
    {
        strncpy(dest_name, new_name, MEASURE_NAME_NULLED_LEN);
    }
}


static void _measurements_update_def(measurements_def_t* def)
{
    if (def->type == PULSE_COUNT)
        _measurements_replace_name_if_legacy(def->name, MEASUREMENTS_LEGACY_PULSE_COUNT_NAME, MEASUREMENTS_PULSE_COUNT_NAME_1);
}


void measurements_init(void)
{
    _measurements_arr.def = persist_measurements.measurements_arr;

    unsigned found = 0;

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurements_def_t* def = &_measurements_arr.def[n];
        unsigned char id_start = def->name[0];

        if (!id_start || id_start == 0xFF)
        {
            def->name[0] = 0;
            continue;
        }

        found++;
        _measurements_update_def(def);
    }

    if (!found)
    {
        log_error("No persistent loaded, load defaults.");
        model_measurements_add_defaults(_measurements_arr.def);
        ios_measurements_init();
    }
    else measurements_debug("Loading measurements.");

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurements_def_t* def = &_measurements_arr.def[n];
        measurements_data_t* data = &_measurements_arr.data[n];

        if (!def->name[0])
            continue;

        measurements_inf_t inf;
        if (!model_measurements_get_inf(def, data, &inf))
            continue;
        _measurements_arr.data[n].collection_time_cache = _measurements_get_collection_time(def, &inf);
        if (inf.enable_cb)
            inf.enable_cb(def->name, def->interval > 0);
    }

    if (!found)
        persist_commit();

    transmit_interval = persist_data.model_config.mins_interval;

    if (transmit_interval % 1000)
        measurements_debug("Loading interval of %"PRIu32".%03"PRIu32" minutes", transmit_interval/1000, transmit_interval%1000);
    else
        measurements_debug("Loading interval of %"PRIu32" minutes", transmit_interval/1000);
}


void measurements_set_debug_mode(bool enable)
{
    if (enable)
        measurements_debug("Enabling measurements debug mode.");
    else
        measurements_debug("Disabling measurements debug mode.");
    _measurements_debug_mode = enable;
}


void measurements_power_mode(measurements_power_mode_t mode)
{
    _measurements_power_mode = mode;
}

typedef struct
{
    measurements_info_t        base;
    measurements_reading_t*    reading;
    measurements_value_type_t* type;
    bool                       func_success;
} _measurements_get_reading_packet_t;


static bool _measurements_get_reading_iteration(void* userdata)
{
    _measurements_get_reading_packet_t* info = (_measurements_get_reading_packet_t*)userdata;
    if (!info->base.inf->iteration_cb)
        return false;
    return (info->base.inf->iteration_cb(info->base.def->name) == MEASUREMENTS_SENSOR_STATE_SUCCESS);
}


static bool _measurements_get_reading_collection(void* userdata)
{
    _measurements_get_reading_packet_t* info = (_measurements_get_reading_packet_t*)userdata;

    measurements_sensor_state_t resp = info->base.inf->get_cb(info->base.def->name, info->reading);
    info->base.data->is_collecting = 0;
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            *(info->type) = info->base.data->value_type;
            info->func_success = true;
            return true;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("Collect function returned an error.");
            *(info->type) = MEASUREMENTS_VALUE_TYPE_INVALID;
            info->func_success = false;
            return true;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            break;
        default:
            measurements_debug("Unknown response from collect function.");
            info->func_success = false;
            return true;
    }
    info->func_success = true;
    return false;
}


static bool _measurements_get_reading2(measurements_def_t* def, measurements_data_t* data, measurements_reading_t* reading, measurements_value_type_t* type)
{
    if (!def|| !data || !reading)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }

    if (data->is_collecting)
    {
        measurements_debug("Measurement already being collected.");
        return false;
    }

    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Could not get measurement interface.");
        return false;
    }

    if (inf.init_cb && inf.init_cb(def->name, true) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        measurements_debug("Could not begin the measurement.");
        goto bad_exit;
    }
    data->collection_time_cache = _measurements_get_collection_time(def, &inf);

    data->is_collecting = 1;
    uint32_t init_time = get_since_boot_ms();

    _measurements_get_reading_packet_t info = {{def, data, &inf}, reading, type, false};
    bool iterate_success = main_loop_iterate_for(data->collection_time_cache, _measurements_get_reading_iteration, &info);
    if (inf.iteration_cb && !iterate_success)
    {
        measurements_debug("Failed on iterate.");
        goto bad_exit;
    }

    uint32_t time_taken = since_boot_delta(get_since_boot_ms(), init_time);
    uint32_t time_remaining = (time_taken > data->collection_time_cache)?0:(data->collection_time_cache - time_taken);

    measurements_debug("Time taken: %"PRIu32, time_taken);

    measurements_debug("Waiting for collection.");
    info.func_success = false;
    if (!main_loop_iterate_for(time_remaining, _measurements_get_reading_collection, &info))
    {
        measurements_debug("Failed on collect.");
        goto bad_exit;
    }

    return info.func_success;
bad_exit:
    data->is_collecting = 0;
    return false;
}


bool measurements_get_reading(char* measurement_name, measurements_reading_t* reading, measurements_value_type_t* type)
{
    if (!measurement_name || !reading)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    measurements_def_t* def;
    measurements_data_t* data;
    if (!measurements_get_measurements_def(measurement_name, &def, &data))
    {
        measurements_debug("Could not get measurement definition and data.");
        return false;
    }
    return _measurements_get_reading2(def, data, reading, type);
}


bool measurements_reading_to_str(measurements_reading_t* reading, measurements_value_type_t type, char* text, uint8_t len)
{
    if (!reading || !text)
        return false;
    switch(type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            snprintf(text, len, "%"PRIi64, reading->v_i64);
            return true;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
        {
            uint32_t decimal = reading->v_f32 % 1000;
            if (reading->v_f32 < 0)
                decimal = (-reading->v_f32) % 1000;
            snprintf(text, len, "%"PRIi32".%03"PRIi32, reading->v_f32 / 1000, decimal);
            return true;
        }
        case MEASUREMENTS_VALUE_TYPE_STR:
            snprintf(text, len, "\"%s\"", reading->v_str);
            return true;
        default:
            break;
    }
    return false;
}


bool measurements_rename(char* orig_name, char* new_name_raw)
{
    if (!orig_name || !new_name_raw)
    {
        measurements_debug("Handed a NULL pointer.");
        return false;
    }
    char new_name[MEASURE_NAME_NULLED_LEN] = {0};
    strncpy(new_name, new_name_raw, MEASURE_NAME_LEN);
    if (measurements_get_measurements_def(new_name, NULL, NULL))
    {
        measurements_debug("Measurement with new name already exists.");
        return false;
    }
    measurements_def_t* def;
    if (!measurements_get_measurements_def(orig_name, &def, NULL))
    {
        measurements_debug("Can not get the measurements def.");
        return false;
    }
    strncpy(def->name, new_name, MEASURE_NAME_NULLED_LEN);
    return true;
}


static command_response_t _measurements_cb(char *args)
{
    measurements_print();
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_enable_cb(char *args)
{
    if (args[0])
        measurements_enabled = (args[0] == '1');

    log_out("measurements_enabled : %c", (measurements_enabled)?'1':'0');
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_get_cb(char* args)
{
    char * p = skip_space(args);
    measurements_reading_t reading;
    measurements_value_type_t type;
    if (!measurements_get_reading(p, &reading, &type))
    {
        log_out("Failed to get measurement reading.");
        return COMMAND_RESP_ERR;
    }
    char text[16];
    if (!measurements_reading_to_str(&reading, type, text, 16))
    {
        log_out("Could not convert the reading to a string.");
        return COMMAND_RESP_ERR;
    }
    log_out("%s: %s", p, text);
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_no_comms_cb(char* args)
{
    bool enable = strtoul(args, NULL, 10);
    measurements_set_debug_mode(enable);
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_interval_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_interval = strtoul(p, NULL, 10);

        if (measurements_set_interval(name, new_interval))
        {
            log_out("Changed %s interval to %"PRIu8, name, new_interval);
            return COMMAND_RESP_OK;
        }
        else
        {
            log_out("Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
    else
    {
        uint8_t interval;
        if (measurements_get_interval(name, &interval))
        {
            log_out("Interval of %s = %"PRIu8, name, interval);
            return COMMAND_RESP_OK;
        }
        else
        {
            log_out("Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
}


static command_response_t _measurements_samplecount_cb(char * args)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit(p[0]))
    {
        uint8_t new_samplecount = strtoul(p, NULL, 10);

        if (measurements_set_samplecount(name, new_samplecount))
        {
            log_out("Changed %s samplecount to %"PRIu8, name, new_samplecount);
            return COMMAND_RESP_OK;
        }
        else
        {
            log_out("Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
    else
    {
        uint8_t samplecount;
        if (measurements_get_samplecount(name, &samplecount))
        {
            log_out("Samplecount of %s = %"PRIu8, name, samplecount);
            return COMMAND_RESP_OK;
        }
        else
        {
            log_out("Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
}


static command_response_t _measurements_repop_cb(char* args)
{
    model_measurements_repopulate();
    log_out("Repopulated measurements.");
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_interval_mins_cb(char* args)
{
    if (args[0])
    {
        double new_interval_mins_f = strtod(args, NULL);
        if (!new_interval_mins_f)
            new_interval_mins_f = 5.;

        uint32_t new_interval_mins = (new_interval_mins_f * 1000.);

        if (new_interval_mins % 1000)
            log_out("Setting interval minutes to %"PRIu32".03%"PRIu32, new_interval_mins/1000, new_interval_mins%1000);
        else
            log_out("Setting interval minutes to %"PRIu32, new_interval_mins/1000);
        persist_data.model_config.mins_interval = new_interval_mins;
        transmit_interval = new_interval_mins;
    }
    else
    {
        if (transmit_interval % 1000)
            log_out("Current interval minutes is %"PRIu32".03%"PRIu32, transmit_interval/1000, transmit_interval%1000);
        else
            log_out("Current interval minutes is %"PRIu32, transmit_interval/1000);
    }
    return COMMAND_RESP_OK;
}


struct cmd_link_t* measurements_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "measurements", "Print measurements",                  _measurements_cb                , false , NULL },
                                       { "meas_enable",  "Enable measuremnts.",                 _measurements_enable_cb         , false , NULL },
                                       { "get_meas",     "Get a measurement",                   _measurements_get_cb            , false , NULL },
                                       { "no_comms",     "Dont need comms for measurements",    _measurements_no_comms_cb       , false , NULL },
                                       { "interval",     "Set the interval",                    _measurements_interval_cb       , false , NULL },
                                       { "samplecount",  "Set the samplecount",                 _measurements_samplecount_cb    , false , NULL },
                                       { "interval_mins","Get/Set interval minutes",            _measurements_interval_mins_cb  , false , NULL },
                                       { "repop",        "Repopulate measurements.",            _measurements_repop_cb          , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
