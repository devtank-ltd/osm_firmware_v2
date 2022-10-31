#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "measurements.h"
#include "comms.h"
#include "log.h"
#include "config.h"
#include "hpm.h"
#include "cc.h"
#include "bat.h"
#include "common.h"
#include "persist_config.h"
#include "modbus_measurements.h"
#include "ds18b20.h"
#include "htu21d.h"
#include "pulsecount.h"
#include "veml7700.h"
#include "sai.h"
#include "sleep.h"
#include "uart_rings.h"
#include "fw.h"
#include "platform.h"


#define MEASUREMENTS_DEFAULT_COLLECTION_TIME    (uint32_t)1000
#define MEASUREMENTS_VALUE_STR_LEN              23
#define MEASUREMENTS_SEND_STR_LEN               8


#define MEASUREMENTS_SEND_IS_SIGNED    0x10

typedef enum
{
    MEASUREMENTS_SEND_TYPE_UNSET   = 0,
    MEASUREMENTS_SEND_TYPE_UINT8   = 1,
    MEASUREMENTS_SEND_TYPE_UINT16  = 2,
    MEASUREMENTS_SEND_TYPE_UINT32  = 3,
    MEASUREMENTS_SEND_TYPE_UINT64  = 4,
    MEASUREMENTS_SEND_TYPE_INT8    = 1 | MEASUREMENTS_SEND_IS_SIGNED,
    MEASUREMENTS_SEND_TYPE_INT16   = 2 | MEASUREMENTS_SEND_IS_SIGNED,
    MEASUREMENTS_SEND_TYPE_INT32   = 3 | MEASUREMENTS_SEND_IS_SIGNED,
    MEASUREMENTS_SEND_TYPE_INT64   = 4 | MEASUREMENTS_SEND_IS_SIGNED,
    MEASUREMENTS_SEND_TYPE_FLOAT   = 5 | MEASUREMENTS_SEND_IS_SIGNED,
    MEASUREMENTS_SEND_TYPE_STR     = 0x20,
} measurements_send_type_t;


typedef struct
{
    union
    {
        struct
        {
            int64_t sum;
            int64_t max;
            int64_t min;
        } value_64;
        struct
        {
            int32_t sum;
            int32_t max;
            int32_t min;
        } value_f;
        struct
        {
            char    str[MEASUREMENTS_VALUE_STR_LEN];
        } value_s;
    };
} measurements_value_t;


typedef struct
{
    measurements_value_t    value;
    uint8_t                 value_type;                                 /* measurements_value_type_t */
    uint8_t                 num_samples;
    uint8_t                 num_samples_init;
    uint8_t                 num_samples_collected;
    uint32_t                collection_time_cache;
} measurements_data_t;


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
static uint16_t                     _measurements_hex_arr_pos                            =  0;
static measurements_arr_t           _measurements_arr                                    = {0};
static bool                         _measurements_debug_mode                             = false;

bool                                 measurements_enabled                                = true;

static measurements_power_mode_t    _measurements_power_mode                             = MEASUREMENTS_POWER_MODE_AUTO;

static unsigned _measurements_chunk_start_pos = 0;
static unsigned _measurements_chunk_prev_start_pos = 0;


uint32_t transmit_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL; /* in minutes, defaulting to 15 minutes */

#define INTERVAL_TRANSMIT_MS   (transmit_interval * 60 * 1000)


bool _measurements_get_measurements_def(char* name, measurements_def_t ** measurements_def, measurements_data_t ** measurements_data)
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


static bool _measurements_arr_append_float(int32_t val)
{
    return _measurements_arr_append_i32(val);
}


static bool _measurements_arr_append_str(char* val, unsigned len)
{
    if (len > MEASUREMENTS_SEND_STR_LEN)
        len = MEASUREMENTS_SEND_STR_LEN;
    for (unsigned i = 0; i < len; i++)
    {
        if (!_measurements_arr_append_i8((int8_t)(val[i])))
        {
            return false;
        }
    }
    for (unsigned i = len; i < MEASUREMENTS_SEND_STR_LEN; i++)
    {
        if (!_measurements_arr_append_i8((int8_t)0))
        {
            return false;
        }
    }
    return true;
}


static bool _measurements_append_i64(int64_t* value)
{
    measurements_send_type_t compressed_type;
    if (*value > 0)
    {
        if (*value > UINT32_MAX)
            compressed_type = MEASUREMENTS_SEND_TYPE_UINT64;
        else if (*value > UINT16_MAX)
            compressed_type = MEASUREMENTS_SEND_TYPE_UINT32;
        else if (*value > UINT8_MAX)
            compressed_type = MEASUREMENTS_SEND_TYPE_UINT16;
        else
            compressed_type = MEASUREMENTS_SEND_TYPE_UINT8;
    }

    else if (*value > INT32_MAX || *value < INT32_MIN)
        compressed_type = MEASUREMENTS_SEND_TYPE_INT64;
    else if (*value > INT16_MAX || *value < INT16_MIN)
        compressed_type = MEASUREMENTS_SEND_TYPE_INT32;
    else if (*value > INT8_MAX  || *value < INT8_MIN)
        compressed_type = MEASUREMENTS_SEND_TYPE_INT16;
    else
        compressed_type = MEASUREMENTS_SEND_TYPE_INT8;

    if (!_measurements_arr_append_i8(compressed_type))
        return false;
    union
    {
        uint8_t  u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;
    } v;
    switch(compressed_type)
    {
        case MEASUREMENTS_SEND_TYPE_UINT8:
            v.u8    = *value;
            return _measurements_arr_append_i8(*(int8_t*)&v.u8);
        case MEASUREMENTS_SEND_TYPE_INT8:
            return _measurements_arr_append_i8((int8_t)*value);
        case MEASUREMENTS_SEND_TYPE_UINT16:
            v.u16   = *value;
            return _measurements_arr_append_i16(*(int16_t*)&v.u16);
        case MEASUREMENTS_SEND_TYPE_INT16:
            return _measurements_arr_append_i16((int16_t)*value);
        case MEASUREMENTS_SEND_TYPE_UINT32:
            v.u32   = *value;
            return _measurements_arr_append_i32(*(int32_t*)&v.u32);
        case MEASUREMENTS_SEND_TYPE_INT32:
            return _measurements_arr_append_i32((int32_t)*value);
        case MEASUREMENTS_SEND_TYPE_UINT64:
            v.u64   = *value;
            return _measurements_arr_append_i64(*(int64_t*)&v.u64);
        case MEASUREMENTS_SEND_TYPE_INT64:
            return _measurements_arr_append_i64((int64_t)*value);
        default: break;
    }
    return false;
}


static bool _measurements_append_str(char* value)
{
    if (!_measurements_arr_append_i8(MEASUREMENTS_SEND_TYPE_STR))
        return false;
    uint8_t len = strnlen(value, MEASUREMENTS_VALUE_STR_LEN);
    return _measurements_arr_append_str(value, len);
}


static bool _measurements_append_float(int32_t* value)
{
    if (!_measurements_arr_append_i8(MEASUREMENTS_SEND_TYPE_FLOAT))
        return false;
    return _measurements_arr_append_float(*value);
}


static bool _measurements_to_arr_i64(measurements_data_t* data, bool single)
{
    if (single)
        return _measurements_append_i64(&data->value.value_64.sum);
    bool r = false;
    int64_t mean = data->value.value_64.sum / data->num_samples;
    r |= !_measurements_append_i64(&mean);
    r |= !_measurements_append_i64(&data->value.value_64.min);
    r |= !_measurements_append_i64(&data->value.value_64.max);
    return !r;
}


static bool _measurements_to_arr_str(measurements_data_t* data)
{
    return _measurements_append_str(data->value.value_s.str);
}


static bool _measurements_to_arr_float(measurements_data_t* data, bool single)
{
    if (single)
        return _measurements_append_float(&data->value.value_f.sum);
    bool r = false;
    int32_t mean = data->value.value_f.sum / data->num_samples;
    r |= !_measurements_append_float(&mean);
    r |= !_measurements_append_float(&data->value.value_f.min);
    r |= !_measurements_append_float(&data->value.value_f.max);
    return !r;
}


static bool _measurements_to_arr(measurements_def_t* def, measurements_data_t* data)
{
    bool single = def->samplecount == 1;

    bool r = 0;
    r |= !_measurements_arr_append_i32(*(int32_t*)def->name);
    uint8_t datatype = single ? MEASUREMENTS_DATATYPE_SINGLE : MEASUREMENTS_DATATYPE_AVERAGED;
    r |= !_measurements_arr_append_i8(datatype);

    switch(data->value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            r |= !_measurements_to_arr_i64(data, single);
            break;
        case MEASUREMENTS_VALUE_TYPE_STR:
            r |= !_measurements_to_arr_str(data);
            break;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            r |= !_measurements_to_arr_float(data, single);
            break;
        default:
            measurements_debug("Unknown type '%"PRIu8"'.", data->value_type);
            return false;
    }
    return !r;
}


static bool _measurements_send_start(void)
{
    memset(_measurements_hex_arr, 0, MEASUREMENTS_HEX_ARRAY_SIZE);
    _measurements_hex_arr_pos = 0;

    if (!_measurements_arr_append_i8((int8_t)MEASUREMENTS_PAYLOAD_VERSION))
    {
        log_error("Failed to add even version to measurements hex array.");
        _pending_send = false;
        _last_sent_ms = get_since_boot_ms();
        return false;
    }

    return true;
}


bool measurements_send_test(void)
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

    char id[4] = MEASUREMENTS_FW_VERSION;

    bool r = _measurements_arr_append_i32(*(int32_t*)id);
    r &= _measurements_arr_append_i8((int8_t)MEASUREMENTS_DATATYPE_SINGLE);
    r &= fw_version_get(NULL, &v) == MEASUREMENTS_SENSOR_STATE_SUCCESS;
    r &= _measurements_arr_append_str(v.v_str, MEASUREMENTS_VALUE_STR_LEN);

    if (!r)
        measurements_debug("Failed to add to array.");

    measurements_debug("Sending test array.");
    comms_send(_measurements_hex_arr, _measurements_hex_arr_pos+1);

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
            uint16_t prev_measurements_hex_arr_pos = _measurements_hex_arr_pos;
            if (_measurements_hex_arr_pos >= (comms_get_mtu() / 2) ||
                !_measurements_to_arr(def, data))
            {
                _measurements_hex_arr_pos = prev_measurements_hex_arr_pos;
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
            for (unsigned j = 0; j < _measurements_hex_arr_pos; j++)
                measurements_debug("Packet %u = 0x%"PRIx8, j, _measurements_hex_arr[j]);
        }
        else
            comms_send(_measurements_hex_arr, _measurements_hex_arr_pos+1);
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


static bool _measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf)
{
    if (!def || !data || !inf)
    {
        measurements_debug("Handed NULL pointer.");
        return false;
    }
    // Optional callbacks: get is not optional, neither is collection
    // time if init given are not optional.
    memset(inf, 0, sizeof(measurements_inf_t));
    switch(def->type)
    {
        case FW_VERSION:
            inf->get_cb             = fw_version_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_STR;
            break;
        case PM10:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm10;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case PM25:
            inf->collection_time_cb = hpm_collection_time;
            inf->init_cb            = hpm_init;
            inf->get_cb             = hpm_get_pm25;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case MODBUS:
            inf->collection_time_cb = modbus_measurements_collection_time;
            inf->init_cb            = modbus_measurements_init;
            inf->get_cb             = modbus_measurements_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case CURRENT_CLAMP:
            inf->collection_time_cb = cc_collection_time;
            inf->init_cb            = cc_begin;
            inf->get_cb             = cc_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case W1_PROBE:
            inf->collection_time_cb = ds18b20_collection_time;
            inf->init_cb            = ds18b20_measurements_init;
            inf->get_cb             = ds18b20_measurements_collect;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_FLOAT;
            break;
        case HTU21D_TMP:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_temp_measurements_init;
            inf->get_cb             = htu21d_temp_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case HTU21D_HUM:
            inf->collection_time_cb = htu21d_measurements_collection_time;
            inf->init_cb            = htu21d_humi_measurements_init;
            inf->get_cb             = htu21d_humi_measurements_get;
            inf->iteration_cb       = htu21d_measurements_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case BAT_MON:
            inf->collection_time_cb = bat_collection_time;
            inf->init_cb            = bat_begin;
            inf->get_cb             = bat_get;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case PULSE_COUNT:
            inf->collection_time_cb = pulsecount_collection_time;
            inf->init_cb            = pulsecount_begin;
            inf->get_cb             = pulsecount_get;
            inf->acked_cb           = pulsecount_ack;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case LIGHT:
            inf->collection_time_cb = veml7700_measurements_collection_time;
            inf->init_cb            = veml7700_light_measurements_init;
            inf->get_cb             = veml7700_light_measurements_get;
            inf->iteration_cb       = veml7700_iteration;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        case SOUND:
            inf->collection_time_cb = sai_collection_time;
            inf->init_cb            = sai_measurements_init;
            inf->get_cb             = sai_measurements_get;
            inf->iteration_cb       = sai_iteration_callback;
            data->value_type        = MEASUREMENTS_VALUE_TYPE_I64;
            break;
        default:
            log_error("Unknown measurements type! : 0x%"PRIx8, def->type);
    }
    return true;
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
        measurements_data_t* data = &_measurements_arr.data[i];
        if (!def->name[0])
            continue;
        measurements_inf_t inf;
        if (!_measurements_get_inf(def, data, &inf))
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
    if (!_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Failed to get the interface for %s.", def->name);
        data->num_samples_init++;
        data->num_samples_collected++;
        return;
    }
    if (!inf.init_cb)
    {
        // Init functions are optional
        data->num_samples_init++;
        measurements_debug("%s has no init function (optional).", def->name);
        return;
    }
    measurements_sensor_state_t resp = inf.init_cb(def->name);
    switch(resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            measurements_debug("%s successfully init'd.", def->name);
            data->num_samples_init++;
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
    if (!_measurements_get_inf(def, data, &inf))
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
    measurements_debug("Value : %"PRIi32, new_value.v_f32);

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
    measurements_debug("Sum : %"PRIi32, data->value.value_f.sum);
    measurements_debug("Min : %"PRIi32, data->value.value_f.min);
    measurements_debug("Max : %"PRIi32, data->value.value_f.max);

    return true;
}


static bool _measurements_sample_get_iteration(measurements_def_t* def, measurements_data_t* data)
{
    measurements_inf_t inf;
    if (!_measurements_get_inf(def, data, &inf))
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


void measurements_derive_cc_phase(void)
{
    unsigned len = 0;
    adcs_type_t clamps[ADC_COUNT] = {0};
    measurements_def_t* def;
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_1_NAME, &def, NULL) &&
        _measurements_def_is_active(def) )
    {
        clamps[len++] = ADCS_TYPE_CC_CLAMP1;
    }
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_2_NAME, &def, NULL) &&
        _measurements_def_is_active(def) )
    {
        clamps[len++] = ADCS_TYPE_CC_CLAMP2;
    }
    if (_measurements_get_measurements_def(MEASUREMENTS_CURRENT_CLAMP_3_NAME, &def, NULL) &&
        _measurements_def_is_active(def) )
    {
        clamps[len++] = ADCS_TYPE_CC_CLAMP3;
    }
    cc_set_active_clamps(clamps, len);
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
            if (!_measurements_get_inf(def, data, &inf))
                return false;
            data->collection_time_cache = _measurements_get_collection_time(def, &inf);
        }
        measurements_derive_cc_phase();
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
            memset(&_measurements_arr.def[i], 0, sizeof(measurements_def_t));
            memset(&_measurements_arr.data[i], 0, sizeof(measurements_data_t));
            measurements_derive_cc_phase();
            return true;
        }
    }
    return false;
}


bool measurements_set_interval(char* name, uint8_t interval)
{
    measurements_def_t* measurements_def = NULL;
    if (!_measurements_get_measurements_def(name, &measurements_def, NULL))
    {
        return false;
    }
    measurements_def->interval = interval;
    measurements_derive_cc_phase();
    return true;
}


bool measurements_get_interval(char* name, uint8_t * interval)
{
    measurements_def_t* measurements_def = NULL;
    if (!interval || !_measurements_get_measurements_def(name, &measurements_def, NULL))
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
    if (!_measurements_get_measurements_def(name, &measurements_def, NULL))
    {
        return false;
    }
    measurements_def->samplecount = samplecount;
    measurements_derive_cc_phase();
    return true;
}


bool measurements_get_samplecount(char* name, uint8_t * samplecount)
{
    measurements_def_t* measurements_def = NULL;
    if (!samplecount || !_measurements_get_measurements_def(name, &measurements_def, NULL))
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
        log_out("%s\t%"PRIu8"x%"PRIu32"mins\t\t%"PRIu8, measurements_def->name, measurements_def->interval, transmit_interval, measurements_def->samplecount);
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
    _measurements_arr.def = persist_get_measurements();

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
        measurements_add_defaults(_measurements_arr.def);
    }
    else measurements_debug("Loading measurements.");

    for(unsigned n = 0; n < MEASUREMENTS_MAX_NUMBER; n++)
    {
        measurements_def_t* def = &_measurements_arr.def[n];
        measurements_data_t* data = &_measurements_arr.data[n];

        if (!def->name[0])
            continue;

        measurements_inf_t inf;
        if (!_measurements_get_inf(def, data, &inf))
            continue;
        _measurements_arr.data[n].collection_time_cache = _measurements_get_collection_time(def, &inf);
    }

    if (!found)
        persist_commit();

    if (persist_get_mins_interval(&transmit_interval))
    {
        if (!transmit_interval)
            transmit_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;

        measurements_debug("Loading interval of %"PRIu32" minutes", transmit_interval);
    }
    else
    {
        log_error("Could not load measurements interval.");
        transmit_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
    }
    measurements_derive_cc_phase();
}


void measurements_repopulate(void)
{
    measurements_def_t def;
    measurements_setup_default(&def, MEASUREMENTS_FW_VERSION,           4,  1,  FW_VERSION      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM10_NAME,            0,  5,  PM10            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PM25_NAME,            0,  5,  PM25            );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_1_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_2_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_CURRENT_CLAMP_3_NAME, 0,  25, CURRENT_CLAMP   );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_W1_PROBE_NAME_1,      0,  5,  W1_PROBE        );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_TEMP,          1,  2,  HTU21D_TMP      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_HTU21D_HUMI,          1,  2,  HTU21D_HUM      );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_BATMON_NAME,          1,  5,  BAT_MON         );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_1,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_PULSE_COUNT_NAME_2,   0,  1,  PULSE_COUNT     );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_LIGHT_NAME,           1,  5,  LIGHT           );
    measurements_add(&def);
    measurements_setup_default(&def, MEASUREMENTS_SOUND_NAME,           1,  5,  SOUND           );
    measurements_add(&def);
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
} _measurements_get_reading_packet_t;


static bool _measurements_get_reading_iteration(void* userdata)
{
    _measurements_get_reading_packet_t* info = (_measurements_get_reading_packet_t*)userdata;
    if (!info->base.inf->iteration_cb)
        return true;
    return (info->base.inf->iteration_cb(info->base.def->name) == MEASUREMENTS_SENSOR_STATE_SUCCESS);
}


static bool _measurements_get_reading_collection(void* userdata)
{
    _measurements_get_reading_packet_t* info = (_measurements_get_reading_packet_t*)userdata;

    measurements_sensor_state_t resp = info->base.inf->get_cb(info->base.def->name, info->reading);
    switch (resp)
    {
        case MEASUREMENTS_SENSOR_STATE_SUCCESS:
            *(info->type) = info->base.data->value_type;
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


bool measurements_get_reading(char* measurement_name, measurements_reading_t* reading, measurements_value_type_t* type)
{
    if (!measurement_name || !reading)
    {
        measurements_debug("Handed NULL pointer.");
        goto bad_exit;
    }
    measurements_def_t* def;
    measurements_data_t* data;
    if (!_measurements_get_measurements_def(measurement_name, &def, &data))
    {
        measurements_debug("Could not get measurement definition and data.");
        goto bad_exit;
    }
    measurements_inf_t inf;
    if (!_measurements_get_inf(def, data, &inf))
    {
        measurements_debug("Could not get measurement interface.");
        goto bad_exit;
    }

    adcs_type_t clamps;
    if (strncmp(def->name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_LEN) == 0)
    {
        clamps = ADCS_TYPE_CC_CLAMP1;
        cc_set_active_clamps(&clamps, 1);
    }
    else if (strncmp(def->name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_LEN) == 0)
    {
        clamps = ADCS_TYPE_CC_CLAMP2;
        cc_set_active_clamps(&clamps, 1);
    }
    else if (strncmp(def->name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_LEN) == 0)
    {
        clamps = ADCS_TYPE_CC_CLAMP3;
        cc_set_active_clamps(&clamps, 1);
    }

    if (inf.init_cb && inf.init_cb(def->name) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        measurements_debug("Could not begin the measurement.");
        goto bad_exit;
    }

    uint32_t init_time = get_since_boot_ms();

    _measurements_get_reading_packet_t info = {{def, data, &inf}, reading, type};
    if (!main_loop_iterate_for(data->collection_time_cache, _measurements_get_reading_iteration, &info))
    {
        measurements_debug("Failed on iterate.");
        goto bad_exit;
    }

    uint32_t time_taken = since_boot_delta(get_since_boot_ms(), init_time);
    uint32_t time_remaining = (time_taken > data->collection_time_cache)?0:(data->collection_time_cache - time_taken);

    measurements_debug("Waiting for collection.");
    if (!main_loop_iterate_for(time_remaining, _measurements_get_reading_collection, &info))
    {
        measurements_debug("Failed on collect.");
        goto bad_exit;
    }

    measurements_derive_cc_phase();
    return true;
bad_exit:
    measurements_derive_cc_phase();
    return false;
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
            strncpy(text, reading->v_str, len-1);
            return true;
        default:
            break;
    }
    return false;
}
