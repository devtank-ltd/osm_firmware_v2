#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <osm/core/measurements.h>
#include <osm/core/log.h>
#include <osm/core/config.h>
#include <osm/core/common.h>
#include <osm/core/persist_config.h>
#include <osm/core/sleep.h>
#include <osm/core/uart_rings.h>
#include <osm/sensors/bat.h>
#include <osm/core/platform.h>
#include "platform_model.h"
#include <osm/protocols/protocol.h>


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
static measurements_arr_t           _measurements_arr                                    = {0};

bool                                 measurements_enabled                                = true;

static measurements_power_mode_t    _measurements_power_mode                             = MEASUREMENTS_POWER_MODE_AUTO;

static unsigned _measurements_chunk_start_pos = 0;
static unsigned _measurements_chunk_prev_start_pos = 0;


uint32_t transmit_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL; /* in minutes, defaulting to 15 minutes */

#define INTERVAL_TRANSMIT_MS   (transmit_interval * 60)

#define MEASUREMENTS_MIN_TRANSMIT_MS                (15 * 1000)


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
    if (!protocol_init())
    {
        _pending_send = false;
        _last_sent_ms = get_since_boot_ms();
        return false;
    }

    return true;
}


bool measurements_send_test(char * name)
{
    if (!protocol_get_connected() || !protocol_send_ready())
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
        protocol_send();
    }
    else measurements_debug("Failed to add to array.");

    return r;
}


static void _measurements_reset_send(void)
{
    measurements_debug("Protocol reset");
    protocol_reset();
    _measurements_chunk_start_pos = _measurements_chunk_prev_start_pos = 0;
    _pending_send = false;
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
        _measurements_arr.data[i].has_sent = false;
}


static void _measurements_send(void)
{
    uint16_t            num_qd = 0;

    static bool has_printed_no_con = false;

    if (!protocol_get_connected())
    {
        if (!has_printed_no_con)
        {
            measurements_debug("Not connected to send, dropping readings");
            has_printed_no_con = true;
            _measurements_chunk_start_pos = _measurements_chunk_prev_start_pos = 0;
        }
        _pending_send = false;
        return;
    }

    has_printed_no_con = false;

    if (!protocol_send_ready())
    {
        if (protocol_send_allowed())
        {
            // Tried to send but not allowed (receiving FW?)
            return;
        }
        if (_pending_send)
        {
            if (since_boot_delta(get_since_boot_ms(), _last_sent_ms) > INTERVAL_TRANSMIT_MS/4)
            {
                measurements_debug("Pending send timed out.");
                _measurements_reset_send();
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
            data->has_sent = true;
            num_qd++;
            memset(&data->value, 0, sizeof(measurements_value_t));
            data->num_samples = 0;
            data->num_samples_init = 0;
            data->num_samples_collected = 0;
        }
    }
    bool is_max = i == MEASUREMENTS_MAX_NUMBER;
    if (is_max)
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
        _pending_send = !is_max;
        bool send_ret = protocol_send();
        if (!send_ret)
        {
            measurements_debug("Protocol send failed, resetting protocol");
            _measurements_reset_send();
            return;
        }
        if (is_max)
            measurements_debug("Complete send");
        else
            measurements_debug("Fragment send, wait to send more.");
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


void on_protocol_sent_ack(bool ack)
{
    if (!ack)
    {
        _measurements_chunk_prev_start_pos = _measurements_chunk_start_pos = 0;
         _pending_send = false;
        return;
    }
    unsigned start = _measurements_chunk_prev_start_pos;
    unsigned end = _measurements_chunk_start_pos ? _measurements_chunk_start_pos : MEASUREMENTS_MAX_NUMBER;
    for (unsigned i = start; i < end; i++)
    {
        measurements_def_t*  def  = &_measurements_arr.def[i];
        if (!def->name[0] || !def->interval || (_interval_count % def->interval != 0))
            continue;
        measurements_inf_t inf;
        if (!model_measurements_get_inf(def, NULL, &inf))
            continue;

        if (inf.acked_cb)
            inf.acked_cb(def->name);
        _measurements_arr.data[i].has_sent = false;
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


static measurements_sensor_state_t _measurements_sample_init_iteration(measurements_def_t* def, measurements_data_t* data)
{
    /* returns boolean of not busy/waiting for measurement */
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        data->is_collecting = 0;
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (data->is_collecting)
    {
        measurements_debug("Measurement %s already collecting.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!inf.init_cb)
    {
        // Init functions are optional
        data->num_samples_init++;
        data->is_collecting = 1;
        measurements_debug("%s has no init function (optional).", def->name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
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
            break;
    }
    return resp;
}


static bool _measurements_sample_iteration_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        data->is_collecting = 0;
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
            data->is_collecting = 0;
            return false;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            return true;
    }
    return false;
}


static measurements_sensor_state_t _measurements_sample_get_resp(measurements_def_t* def, measurements_data_t* data, measurements_inf_t* inf, measurements_reading_t* new_value)
{
    if (!def || !data || !inf || !new_value)
    {
        measurements_debug("Handed a NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
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
            break;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            // Sensor was busy, will retry.
            break;
    }
    return resp;
}


static void _measurements_sample_proc_i64(measurements_data_t* data, measurements_reading_t * new_value)
{
    measurements_debug("Value : %"PRIi64, new_value->v_i64);

    if (data->num_samples == 0)
    {
        // If first measurements
        data->num_samples++;
        data->value.value_64.min = new_value->v_i64;
        data->value.value_64.max = new_value->v_i64;
        data->value.value_64.sum = new_value->v_i64;
    }
    else
    {
        data->value.value_64.sum += new_value->v_i64;

        data->num_samples++;

        if (new_value->v_i64 > data->value.value_64.max)
            data->value.value_64.max = new_value->v_i64;

        else if (new_value->v_i64 < data->value.value_64.min)
            data->value.value_64.min = new_value->v_i64;
    }

    measurements_debug("Sum : %"PRIi64, data->value.value_64.sum);
    measurements_debug("Min : %"PRIi64, data->value.value_64.min);
    measurements_debug("Max : %"PRIi64, data->value.value_64.max);
}


static void _measurements_sample_proc_str(measurements_data_t* data, measurements_reading_t * new_value)
{
    uint8_t new_len = strnlen(new_value->v_str, MEASUREMENTS_VALUE_STR_LEN - 1);
    strncpy(data->value.value_s.str, new_value->v_str, new_len);
    data->value.value_s.str[new_len] = 0;
    measurements_debug("Value : %s", data->value.value_s.str);
    data->num_samples++;
}


static void _measurements_sample_proc_float(measurements_data_t* data, measurements_reading_t * new_value)
{
    measurements_debug("Value : %"PRIi32".%03"PRIu32, new_value->v_f32/1000, (uint32_t)abs(new_value->v_f32)%1000);

    if (data->num_samples == 0)
    {
        // If first measurements
        data->num_samples++;
        data->value.value_f.min = new_value->v_f32;
        data->value.value_f.max = new_value->v_f32;
        data->value.value_f.sum = new_value->v_f32;
    }
    else
    {
        data->value.value_f.sum += new_value->v_f32;

        data->num_samples++;

        if (new_value->v_f32 > data->value.value_f.max)
            data->value.value_f.max = new_value->v_f32;

        else if (new_value->v_f32 < data->value.value_f.min)
            data->value.value_f.min = new_value->v_f32;
    }

    measurements_debug("Sum : %"PRIi32".%03"PRIu32, data->value.value_f.sum/1000, (uint32_t)abs(data->value.value_f.sum)%1000);
    measurements_debug("Min : %"PRIi32".%03"PRIu32, data->value.value_f.min/1000, (uint32_t)abs(data->value.value_f.min)%1000);
    measurements_debug("Max : %"PRIi32".%03"PRIu32, data->value.value_f.max/1000, (uint32_t)abs(data->value.value_f.max)%1000);
}


static measurements_sensor_state_t _measurements_sample_get_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!model_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_collected++;
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    if (!inf.get_cb)
    {
        // Get function is non-optional
        data->num_samples_collected++;
        measurements_debug("%s has no collect function.", def->name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    measurements_reading_t new_value;
    measurements_sensor_state_t rsp = _measurements_sample_get_resp(def, data, &inf, &new_value);

    if (rsp == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        switch(data->value_type)
        {
            case MEASUREMENTS_VALUE_TYPE_I64:
                _measurements_sample_proc_i64(data, &new_value);
                break;
            case MEASUREMENTS_VALUE_TYPE_STR:
                _measurements_sample_proc_str(data, &new_value);
                break;
            case MEASUREMENTS_VALUE_TYPE_FLOAT:
                _measurements_sample_proc_float(data, &new_value);
                break;
            default:
                measurements_debug("Unknown type '%"PRIu8"'. Don't know what to do.", data->value_type);
                return false;
        }

        data->collection_time_cache = _measurements_get_collection_time(def, &inf);
    }
    return rsp;
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

        /* is_immediate is only valid if samplecount is 1 */
        bool is_immediate = def->is_immediate && def->samplecount == 1;

        if (is_immediate)
            /* Collect 10 ms before needing to send. */
            time_init_boundary = (data->num_samples_init * sample_interval) + sample_interval - 10;
        else
            time_init_boundary = (data->num_samples_init * sample_interval) + sample_interval/2;

        if (time_init_boundary < data->collection_time_cache)
        {
            // Assert that no negative rollover could happen for long collection times. Just do it immediately.
            data->collection_time_cache = time_init_boundary;
        }

        time_init   = time_init_boundary - data->collection_time_cache;
        if (is_immediate)
            time_collect    = (data->num_samples_collected  * sample_interval) + sample_interval - 10;
        else
            time_collect    = (data->num_samples_collected  * sample_interval) + sample_interval/2;
        if (time_since_interval >= time_init)
        {
            if (data->num_samples_collected < data->num_samples_init)
            {
                data->num_samples_collected++;
                measurements_debug("Could not collect before next init.");
            }
            /* If the init function returned false, then the sensor is
             * busy, so the wait time should be 0 */
            measurements_sensor_state_t init_rsp = _measurements_sample_init_iteration(def, data);
            if (init_rsp == MEASUREMENTS_SENSOR_STATE_BUSY)
            {
                wait_time = 0;
            }
            else
            {
                wait_time = since_boot_delta(data->collection_time_cache + time_init, time_since_interval);
            }
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

        if (data->num_samples_collected == data->num_samples_init)
        {
        }
        else if (data->num_samples_collected + 1 == data->num_samples_init)
        {
            if (time_since_interval >= time_collect )
            {
                measurements_sensor_state_t get_rsp = _measurements_sample_get_iteration(def, data);
                if (get_rsp == MEASUREMENTS_SENSOR_STATE_BUSY)
                {
                    wait_time = 0;
                }
                else
                {
                    wait_time = since_boot_delta(time_collect + sample_interval, data->collection_time_cache + time_since_interval);
                }
            }
            else
            {
                wait_time = since_boot_delta(time_collect, time_since_interval);
            }
        }
        else
        {
            log_error("Collect and init fell out of sync for %s, skipping collects.", def->name);
            data->num_samples_collected = data->num_samples_init;
        }

        if (_check_time.wait_time > wait_time)
        {
            _check_time.wait_time = wait_time;
        }
        uart_rings_out_drain();
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
    unsigned                space = MEASUREMENTS_MAX_NUMBER - 1;
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
                memset(def, 0, sizeof(measurements_def_t));
                memset(data, 0, sizeof(measurements_data_t));
                return false;
            }
            data->collection_time_cache = _measurements_get_collection_time(def, &inf);
            if (inf.enable_cb)
                inf.enable_cb(def->name, def->interval > 0);
        }
        unsigned name_len = strnlen(def->name, MEASURE_NAME_LEN);
        unsigned i;
        for (i = 0; i < name_len; i++)
        {
            if (def->name[i] == ' ')
                def->name[i] = '_';
        }
        for (i = name_len; i < MEASURE_NAME_LEN; i++)
        {
            if (def->name[i] == ' ')
                def->name[i] = '\0';
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


static bool _protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type)
{
    measurements_data_t data =
    {
        .value_type     = type,
        .num_samples    = 1,
    };
    memcpy(&data.value, reading, sizeof(measurements_value_type_t));
    return protocol_append_measurement(def, &data);
}


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

    if (!protocol_init())
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
            if (!_protocol_append_instant_measurement(def, &reading, type))
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
    if (_measurements_chunk_start_pos != MEASUREMENTS_MAX_NUMBER && _measurements_chunk_start_pos != 0)
    {
        measurements_debug("Cannot instant send, there is a measurement send underway.");
        return;
    }
    uint32_t now = get_since_boot_ms();
    /* Add +10 as this is called before measurements_send and to ensure no negative overflow. */
    if (since_boot_delta(_last_sent_ms + INTERVAL_TRANSMIT_MS + 10, now) <= MEASUREMENTS_MIN_TRANSMIT_MS + 10)
    {
        measurements_debug("Cannot send instant send, scheduled uplink soon.");
        return;
    }
    protocol_send();
}


void measurements_loop_iteration(void)
{
    if (!measurements_enabled)
        return;

    static bool has_printed_no_con = false;

    if (!protocol_get_connected())
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

    if (protocol_send_ready())
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
    info->func_success = false;
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            *(info->type) = info->base.data->value_type;
            info->func_success = true;
            return true;
        case MEASUREMENTS_SENSOR_STATE_ERROR:
            measurements_debug("Collect function returned an error.");
            *(info->type) = MEASUREMENTS_VALUE_TYPE_INVALID;
            return true;
        case MEASUREMENTS_SENSOR_STATE_BUSY:
            break;
        default:
            measurements_debug("Unknown response from collect function.");
            return true;
    }
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

    bool was_not_enabled = inf.is_enabled_cb && inf.enable_cb && !inf.is_enabled_cb(def->name);
    if (was_not_enabled)
        inf.enable_cb(def->name, true);

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
    uint32_t time_remaining = (time_taken > data->collection_time_cache)? 0 :(data->collection_time_cache - time_taken);

    measurements_debug("Time taken: %"PRIu32, time_taken);

    measurements_debug("Waiting for collection.");
    info.func_success = false;
    init_time = get_since_boot_ms();
    bool collect_success = main_loop_iterate_for(time_remaining, _measurements_get_reading_collection, &info);

    time_taken = since_boot_delta(get_since_boot_ms(), init_time);
    time_remaining = (time_taken > data->collection_time_cache)?0:(data->collection_time_cache - time_taken);

    measurements_debug("Time taken: %"PRIu32, time_taken);

    if (!collect_success)
    {
        measurements_debug("Failed on collect.");
        goto bad_exit;
    }

    if (!info.func_success)
        measurements_debug("Collect is timed out...");

    if (was_not_enabled)
        inf.enable_cb(def->name, false);

    return info.func_success;
bad_exit:
    if (was_not_enabled)
        inf.enable_cb(def->name, false);
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


static command_response_t _measurements_cb(char *args, cmd_ctx_t * ctx)
{
    measurements_def_t* measurements_def;
    cmd_ctx_out(ctx,"Loaded Measurements");
    cmd_ctx_out(ctx,"Name\tInterval\tSample Count");
    for (unsigned i = 0; i < MEASUREMENTS_MAX_NUMBER; i++)
    {
        measurements_def = &_measurements_arr.def[i];
        unsigned char id_start = measurements_def->name[0];
        if (!id_start || id_start == 0xFF)
            continue;
        if (transmit_interval % 1000)
            cmd_ctx_out(ctx,"%s\t%"PRIu8"x%"PRIu32".%03"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval/1000, transmit_interval%1000, measurements_def->samplecount);
        else
            cmd_ctx_out(ctx,"%s\t%"PRIu8"x%"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval/1000, measurements_def->samplecount);
    }
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_enable_cb(char *args, cmd_ctx_t * ctx)
{
    bool was_enabled = measurements_enabled;
    if (args[0])
        measurements_enabled = (args[0] == '1');
    if (!was_enabled && measurements_enabled)
        _last_sent_ms = get_since_boot_ms();
    cmd_ctx_out(ctx,"measurements_enabled : %c", (measurements_enabled)?'1':'0');
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_get_cb(char* args, cmd_ctx_t * ctx)
{
    char * p = skip_space(args);
    char name[MEASURE_NAME_NULLED_LEN];
    unsigned len = strnlen(p, MEASURE_NAME_LEN);
    char* end_first_word = strchr(p, ' ');
    if (end_first_word)
        len = end_first_word - p;
    strncpy(name, p, len);
    name[len] = 0;
    measurements_reading_t reading;
    measurements_value_type_t type;
    if (!measurements_get_reading(name, &reading, &type))
    {
        cmd_ctx_error(ctx,"Failed to get measurement reading.");
        return COMMAND_RESP_ERR;
    }
    char text[16];
    if (!measurements_reading_to_str(&reading, type, text, 16))
    {
        cmd_ctx_error(ctx,"Could not convert the reading to a string.");
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx,"%s: %s", name, text);
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_get_to_cb(char* args, cmd_ctx_t * ctx)
{
    char * p = skip_space(args);

    measurements_def_t* def;
    measurements_inf_t inf;
    if (!measurements_get_measurements_def(p, &def, NULL) ||
        !model_measurements_get_inf(def, NULL, &inf))
    {
        cmd_ctx_error(ctx,"Failed to get measurement details of \"%s\"", p);
        return COMMAND_RESP_ERR;
    }

    cmd_ctx_out(ctx,"%s : %u", p, (unsigned)(_measurements_get_collection_time(def, &inf) * 1.5));
    return COMMAND_RESP_OK;
}


static const char* measurements_type_to_str(measurements_def_type_t type)
{
    static const char modbus_name[]         = MEASUREMENTS_DEF_NAME_MODBUS;
    static const char pm10_name[]           = MEASUREMENTS_DEF_NAME_PM10;
    static const char pm25_name[]           = MEASUREMENTS_DEF_NAME_PM25;
    static const char current_clamp_name[]  = MEASUREMENTS_DEF_NAME_CURRENT_CLAMP;
    static const char w1_probe_name[]       = MEASUREMENTS_DEF_NAME_W1_PROBE;
    static const char htu21d_hum_name[]     = MEASUREMENTS_DEF_NAME_HTU21D_HUM;
    static const char htu21d_tmp_name[]     = MEASUREMENTS_DEF_NAME_HTU21D_TMP;
    static const char bat_mon_name[]        = MEASUREMENTS_DEF_NAME_BAT_MON;
    static const char pulse_count_name[]    = MEASUREMENTS_DEF_NAME_PULSE_COUNT;
    static const char light_name[]          = MEASUREMENTS_DEF_NAME_LIGHT;
    static const char sound_name[]          = MEASUREMENTS_DEF_NAME_SOUND;
    static const char fw_version_name[]     = MEASUREMENTS_DEF_NAME_FW_VERSION;
    static const char config_revision_name[] = MEASUREMENTS_DEF_NAME_CONFIG_REVISION;
    static const char ftma_name[]           = MEASUREMENTS_DEF_NAME_FTMA;
    static const char custom_0_name[]       = MEASUREMENTS_DEF_NAME_CUSTOM_0;
    static const char custom_1_name[]       = MEASUREMENTS_DEF_NAME_CUSTOM_1;
    static const char io_reading_name[]     = MEASUREMENTS_DEF_NAME_IO_READING;

    switch (type)
    {
        case MODBUS:
            return modbus_name;
        case PM10:
            return pm10_name;
        case PM25:
            return pm25_name;
        case CURRENT_CLAMP:
            return current_clamp_name;
        case W1_PROBE:
            return w1_probe_name;
        case HTU21D_HUM:
            return htu21d_hum_name;
        case HTU21D_TMP:
            return htu21d_tmp_name;
        case BAT_MON:
            return bat_mon_name;
        case PULSE_COUNT:
            return pulse_count_name;
        case LIGHT:
            return light_name;
        case SOUND:
            return sound_name;
        case FW_VERSION:
            return fw_version_name;
        case CONFIG_REVISION:
            return config_revision_name;
        case FTMA:
            return ftma_name;
        case CUSTOM_0:
            return custom_0_name;
        case CUSTOM_1:
            return custom_1_name;
        case IO_READING:
            return io_reading_name;
        default:
            break;
    }
    return NULL;
}


static command_response_t _measurements_get_type_cb(char* args, cmd_ctx_t * ctx)
{
    char* p = skip_space(args);
    char name[MEASURE_NAME_NULLED_LEN];
    char* np = strchr(p, ' ');
    unsigned len;
    if (!np)
    {
        len = strnlen(p, MEASURE_NAME_LEN);
    }
    else
    {
        len = np - p;
    }
    strncpy(name, p, len+1);

    measurements_def_t* def;
    if (!measurements_get_measurements_def(name, &def, NULL))
    {
        cmd_ctx_error(ctx,"Failed to get measurement details of \"%s\"", p);
        return COMMAND_RESP_ERR;
    }
    const char* type_str = measurements_type_to_str(def->type);
    if (!type_str)
    {
        cmd_ctx_error(ctx,"Unknown measurement type for '%s' (%"PRIu8")", name, def->type);
        return COMMAND_RESP_ERR;
    }
    cmd_ctx_out(ctx,"%s: %s", name, type_str);
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_interval_cb(char * args, cmd_ctx_t * ctx)
{
    char* name = args;
    char* p = skip_space(args);
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit((int)p[0]))
    {
        uint8_t new_interval = strtoul(p, NULL, 10);

        if (measurements_set_interval(name, new_interval))
        {
            cmd_ctx_out(ctx,"Changed %s interval to %"PRIu8, name, new_interval);
            return COMMAND_RESP_OK;
        }
        else
        {
            cmd_ctx_error(ctx,"Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
    else
    {
        uint8_t interval;
        if (measurements_get_interval(name, &interval))
        {
            cmd_ctx_out(ctx,"Interval of %s = %"PRIu8, name, interval);
            return COMMAND_RESP_OK;
        }
        else
        {
            cmd_ctx_error(ctx,"Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
}


static command_response_t _measurements_samplecount_cb(char * args, cmd_ctx_t * ctx)
{
    char* p = skip_space(args);
    char* name = p;
    p = strchr(p, ' ');
    if (p)
    {
        p[0] = 0;
        p = skip_space(p+1);
    }
    if (p && isdigit((int)p[0]))
    {
        uint8_t new_samplecount = strtoul(p, NULL, 10);

        if (measurements_set_samplecount(name, new_samplecount))
        {
            cmd_ctx_out(ctx,"Changed %s samplecount to %"PRIu8, name, new_samplecount);
            return COMMAND_RESP_OK;
        }
        else
        {
            cmd_ctx_error(ctx,"Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
    else
    {
        uint8_t samplecount;
        if (measurements_get_samplecount(name, &samplecount))
        {
            cmd_ctx_out(ctx,"Samplecount of %s = %"PRIu8, name, samplecount);
            return COMMAND_RESP_OK;
        }
        else
        {
            cmd_ctx_error(ctx,"Unknown measurement");
            return COMMAND_RESP_ERR;
        }
    }
}


static command_response_t _measurements_repop_cb(char* args, cmd_ctx_t * ctx)
{
    model_measurements_repopulate();
    cmd_ctx_out(ctx,"Repopulated measurements.");
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_interval_mins_cb(char* args, cmd_ctx_t * ctx)
{
    if (args[0])
    {
        double new_interval_mins_f = strtod(args, NULL);
        if (!new_interval_mins_f)
            new_interval_mins_f = 5.;

        uint32_t new_interval_mins = (new_interval_mins_f * 1000.);

        if (new_interval_mins % 1000)
            cmd_ctx_out(ctx,"Setting interval minutes to %"PRIu32".%03"PRIu32, new_interval_mins/1000, new_interval_mins%1000);
        else
            cmd_ctx_out(ctx,"Setting interval minutes to %"PRIu32, new_interval_mins/1000);
        persist_data.model_config.mins_interval = new_interval_mins;
        transmit_interval = new_interval_mins;
    }
    else
    {
        if (transmit_interval % 1000)
            cmd_ctx_out(ctx,"Current interval minutes is %"PRIu32".%03"PRIu32, transmit_interval/1000, transmit_interval%1000);
        else
            cmd_ctx_out(ctx,"Current interval minutes is %"PRIu32, transmit_interval/1000);
    }
    return COMMAND_RESP_OK;
}


static command_response_t _measurements_is_immediate_cb(char* args, cmd_ctx_t * ctx)
{
    char name[MEASURE_NAME_NULLED_LEN];
    char* p = strchr(args, ' ');
    unsigned len;
    if (!p)
        len = strnlen(args, MEASURE_NAME_LEN);
    else
        len = p - args;
    strncpy(name, args, len);
    name[len] = 0;
    measurements_def_t* def;
    if (!measurements_get_measurements_def(name, &def, NULL))
    {
        cmd_ctx_error(ctx,"Could not get measurement '%s'", name);
        return COMMAND_RESP_ERR;
    }
    if (!p)
        goto print_out;

    char* np;
    p = skip_space(p);
    uint8_t enabled = strtoul(p, &np, 10) ? 1 : 0;
    if (p == np)
        goto print_out;

    def->is_immediate = enabled;

print_out:
    if (def->is_immediate)
        cmd_ctx_out(ctx,"%s is immediate", def->name);
    else
        cmd_ctx_out(ctx,"%s is not immediate", def->name);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* measurements_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "measurements", "Print measurements",                  _measurements_cb                , false , NULL },
        { "meas_enable",  "Enable measuremnts.",                 _measurements_enable_cb         , false , NULL },
        { "get_meas",     "Get a measurement",                   _measurements_get_cb            , false , NULL },
        { "get_meas_to",  "Get timeout of measurement",          _measurements_get_to_cb         , false , NULL },
        { "get_meas_type","Get the type of measurement",         _measurements_get_type_cb       , false , NULL },
        { "interval",     "Set the interval",                    _measurements_interval_cb       , false , NULL },
        { "samplecount",  "Set the samplecount",                 _measurements_samplecount_cb    , false , NULL },
        { "interval_mins","Get/Set interval minutes",            _measurements_interval_mins_cb  , false , NULL },
        { "repop",        "Repopulate measurements.",            _measurements_repop_cb          , false , NULL },
        { "is_immediate", "Set/unset immediate measurements.",   _measurements_is_immediate_cb   , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
