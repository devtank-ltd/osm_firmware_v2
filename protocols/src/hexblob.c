#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include "log.h"
#include "measurements.h"
#include "comms.h"
#include "platform_model.h"

static int8_t                       _measurements_hex_arr[PROTOCOL_HEX_ARRAY_SIZE]   = {0};

#define PROTOCOL_SEND_STR_LEN               8
#define PROTOCOL_ERR_CODE_NAME                  "ERR"


#define PROTOCOL_SEND_IS_SIGNED             0x10
typedef enum
{
    PROTOCOL_SEND_TYPE_UNSET   = 0,
    PROTOCOL_SEND_TYPE_UINT8   = 1,
    PROTOCOL_SEND_TYPE_UINT16  = 2,
    PROTOCOL_SEND_TYPE_UINT32  = 3,
    PROTOCOL_SEND_TYPE_UINT64  = 4,
    PROTOCOL_SEND_TYPE_INT8    = 1 | PROTOCOL_SEND_IS_SIGNED,
    PROTOCOL_SEND_TYPE_INT16   = 2 | PROTOCOL_SEND_IS_SIGNED,
    PROTOCOL_SEND_TYPE_INT32   = 3 | PROTOCOL_SEND_IS_SIGNED,
    PROTOCOL_SEND_TYPE_INT64   = 4 | PROTOCOL_SEND_IS_SIGNED,
    PROTOCOL_SEND_TYPE_FLOAT   = 5 | PROTOCOL_SEND_IS_SIGNED,
    PROTOCOL_SEND_TYPE_STR     = 0x20,
} protocol_send_type_t;


typedef struct
{
    int8_t*     buf;
    unsigned    buflen;
    unsigned    pos;
} protocol_ctx_t;


static protocol_ctx_t _protocol_ctx =
{
    .buf = NULL,
    .buflen = 0,
    .pos = 0,
};


static bool _protocol_append_i8(int8_t val)
{
    if (!_protocol_ctx.buf || !_protocol_ctx.buflen)
    {
        log_error("Buffer is not inited.");
        return false;
    }

    if (_protocol_ctx.pos >= _protocol_ctx.buflen)
    {
        log_error("Protocol buffer is full.");
        return false;
    }
    _protocol_ctx.buf[_protocol_ctx.pos++] = val;
    return true;
}


static bool _protocol_append_i16(int16_t val)
{
    return _protocol_append_i8(val & 0xFF) &&
           _protocol_append_i8((val >> 8) & 0xFF);
}


static bool _protocol_append_i32(int32_t val)
{
    return _protocol_append_i16(val & 0xFFFF) &&
           _protocol_append_i16((val >> 16) & 0xFFFF);
}


static bool _protocol_append_i64(int64_t val)
{
    return _protocol_append_i32(val & 0xFFFFFFFF) &&
           _protocol_append_i32((val >> 32) & 0xFFFFFFFF);
}


static bool _protocol_append_float(int32_t val)
{
    return _protocol_append_i32(val);
}


static bool _protocol_append_str(char* val, unsigned len)
{
    if (len > PROTOCOL_SEND_STR_LEN)
        len = PROTOCOL_SEND_STR_LEN;
    for (unsigned i = 0; i < len; i++)
    {
        if (!_protocol_append_i8((int8_t)(val[i])))
            return false;
    }
    for (unsigned i = len; i < PROTOCOL_SEND_STR_LEN; i++)
    {
        if (!_protocol_append_i8((int8_t)0))
            return false;
    }
    return true;
}


static bool _protocol_append_data_type_i64(int64_t* value)
{
    protocol_send_type_t compressed_type;
    if (*value > 0)
    {
        if (*value > UINT32_MAX)
            compressed_type = PROTOCOL_SEND_TYPE_UINT64;
        else if (*value > UINT16_MAX)
            compressed_type = PROTOCOL_SEND_TYPE_UINT32;
        else if (*value > UINT8_MAX)
            compressed_type = PROTOCOL_SEND_TYPE_UINT16;
        else
            compressed_type = PROTOCOL_SEND_TYPE_UINT8;
    }

    else if (*value > INT32_MAX || *value < INT32_MIN)
        compressed_type = PROTOCOL_SEND_TYPE_INT64;
    else if (*value > INT16_MAX || *value < INT16_MIN)
        compressed_type = PROTOCOL_SEND_TYPE_INT32;
    else if (*value > INT8_MAX  || *value < INT8_MIN)
        compressed_type = PROTOCOL_SEND_TYPE_INT16;
    else
        compressed_type = PROTOCOL_SEND_TYPE_INT8;

    if (!_protocol_append_i8(compressed_type))
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
        case PROTOCOL_SEND_TYPE_UINT8:
            v.u8    = *value;
            return _protocol_append_i8(*(int8_t*)&v.u8);
        case PROTOCOL_SEND_TYPE_INT8:
            return _protocol_append_i8((int8_t)*value);
        case PROTOCOL_SEND_TYPE_UINT16:
            v.u16   = *value;
            return _protocol_append_i16(*(int16_t*)&v.u16);
        case PROTOCOL_SEND_TYPE_INT16:
            return _protocol_append_i16((int16_t)*value);
        case PROTOCOL_SEND_TYPE_UINT32:
            v.u32   = *value;
            return _protocol_append_i32(*(int32_t*)&v.u32);
        case PROTOCOL_SEND_TYPE_INT32:
            return _protocol_append_i32((int32_t)*value);
        case PROTOCOL_SEND_TYPE_UINT64:
            v.u64   = *value;
            return _protocol_append_i64(*(int64_t*)&v.u64);
        case PROTOCOL_SEND_TYPE_INT64:
            return _protocol_append_i64((int64_t)*value);
        default: break;
    }
    return false;
}


static bool _protocol_append_data_type_str(char* value)
{
    if (!_protocol_append_i8(PROTOCOL_SEND_TYPE_STR))
        return false;
    uint8_t len = strnlen(value, MEASUREMENTS_VALUE_STR_LEN);
    return _protocol_append_str(value, len);
}


static bool _protocol_append_data_type_float(int32_t* value)
{
    if (!_protocol_append_i8(PROTOCOL_SEND_TYPE_FLOAT))
        return false;
    return _protocol_append_float(*value);
}


static bool _protocol_append_value_type_i64(measurements_data_t* data, bool single)
{
    if (single)
        return _protocol_append_data_type_i64(&data->value.value_64.sum);
    bool r = false;
    int64_t mean = data->value.value_64.sum / data->num_samples;
    r |= !_protocol_append_data_type_i64(&mean);
    r |= !_protocol_append_data_type_i64(&data->value.value_64.min);
    r |= !_protocol_append_data_type_i64(&data->value.value_64.max);
    return !r;
}


static bool _protocol_append_value_type_str(measurements_data_t* data)
{
    return _protocol_append_data_type_str(data->value.value_s.str);
}


static bool _protocol_append_value_type_float(measurements_data_t* data, bool single)
{
    if (single)
        return _protocol_append_data_type_float(&data->value.value_f.sum);
    bool r = false;
    int32_t mean = data->value.value_f.sum / data->num_samples;
    r |= !_protocol_append_data_type_float(&mean);
    r |= !_protocol_append_data_type_float(&data->value.value_f.min);
    r |= !_protocol_append_data_type_float(&data->value.value_f.max);
    return !r;
}


bool protocol_append_measurement(measurements_def_t* def, measurements_data_t* data)
{
    bool single = def->samplecount == 1;

    unsigned before_pos = _protocol_ctx.pos;

    bool r = 0;
    r |= !_protocol_append_i32(*(int32_t*)def->name);
    uint8_t datatype = single ? MEASUREMENTS_DATATYPE_SINGLE : MEASUREMENTS_DATATYPE_AVERAGED;
    r |= !_protocol_append_i8(datatype);

    switch(data->value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            r |= !_protocol_append_value_type_i64(data, single);
            break;
        case MEASUREMENTS_VALUE_TYPE_STR:
            r |= !_protocol_append_value_type_str(data);
            break;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            r |= !_protocol_append_value_type_float(data, single);
            break;
        default:
            log_error("Unknown type '%"PRIu8"'.", data->value_type);
            r = true;
            break;
    }
    if (r)
    {
        _protocol_ctx.pos = before_pos;
    }
    return !r;
}


bool protocol_append_error_code(uint8_t err_code)
{
    unsigned before_pos = _protocol_ctx.pos;

    bool r = false;
    char name[MEASURE_NAME_NULLED_LEN] = PROTOCOL_ERR_CODE_NAME;
    r |= !_protocol_append_i32(*(int32_t*)name);
    r |= !_protocol_append_i8(MEASUREMENTS_DATATYPE_SINGLE);
    int64_t err_code64 = err_code;
    r |= !_protocol_append_data_type_i64(&err_code64);
    if (r)
    {
        _protocol_ctx.pos = before_pos;
    }
    return !r;
}


bool protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type)
{
    measurements_data_t data =
    {
        .value_type     = type,
        .num_samples    = 1,
    };
    memcpy(&data.value, reading, sizeof(measurements_value_type_t));
    return protocol_append_measurement(def, &data);
}


static bool _protocol_init(int8_t* buf, unsigned buflen)
{
    memset(buf, 0, buflen);

    _protocol_ctx.buf = buf;
    _protocol_ctx.buflen = buflen;
    _protocol_ctx.pos = 0;

    memset(_protocol_ctx.buf, 0, _protocol_ctx.buflen);
    return _protocol_append_i8((int8_t)MEASUREMENTS_PAYLOAD_VERSION);
}


bool protocol_init(void)
{
    return _protocol_init(_measurements_hex_arr, PROTOCOL_HEX_ARRAY_SIZE);
}


unsigned protocol_get_length(void)
{
    return _protocol_ctx.pos;
}


void        protocol_debug(void)
{
    for (unsigned j = 0; j < protocol_get_length(); j++)
        measurements_debug("Packet %u = 0x%"PRIx8, j, _protocol_ctx.buf[j]);
}


void        protocol_send(void)
{
    comms_send(_protocol_ctx.buf, protocol_get_length());
}


void        protocol_send_error_code(uint8_t err_code)
{
    /* Immediate sent, so temporary use a different memory buffer for protocol. */
    int8_t arr[15] = {0};
    protocol_ctx_t org = _protocol_ctx;
    if (!_protocol_init(arr, sizeof(arr)))
    {
        _protocol_ctx = org;
        comms_debug("Could not init memory protocol.");
        return;
    }
    protocol_append_error_code(err_code);
    comms_send(arr, protocol_get_length());
    _protocol_ctx = org; /* Restore normal memory buffer for protocol. */
}
