#include <ctype.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "model_config.h"
#include "log.h"
#include "measurements.h"
#include "comms.h"
#include "persist_config.h"


#define JSON_CLOSE_SIZE 3


static char _json_buf[JSON_BUF_SIZE];
static unsigned _json_buf_pos = 0;

static bool _protocol_append_meas(char * fmt, ...) PRINTF_FMT_CHECK(1, 2);


static bool _protocol_append_v(char * fmt, va_list ap)
{
    unsigned available = JSON_BUF_SIZE - _json_buf_pos;
    unsigned r = vsnprintf(_json_buf + _json_buf_pos, available, fmt, ap);
    if ((r + JSON_CLOSE_SIZE) >= available)
        return false;
    _json_buf_pos += r;
    return true;
}


static bool _protocol_append(char * fmt, ...) PRINTF_FMT_CHECK(1, 2);
static bool _protocol_append(char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    bool r = _protocol_append_v(fmt, ap);
    va_end(ap);
    return r;
}


static bool _protocol_append_meas(char * fmt, ...)
{
    if (!_json_buf_pos)
        return false;
    if (_json_buf[_json_buf_pos - 1] != '{')
        if (!_protocol_append(","))
            return false;
    va_list ap;
    va_start(ap, fmt);
    bool r = _protocol_append_v(fmt, ap);
    va_end(ap);
    return r;
}


static bool _protocol_append_data_type_float(const char * name, int32_t value)
{
    return _protocol_append_meas("\"%s\":%"PRId32".%03ld", name, value/1000, labs(value%1000));
}


static bool _protocol_append_data_type_i64(const char * name, int64_t value)
{
    return _protocol_append_meas("\"%s\":%"PRId64, name, value);
}


static bool _protocol_append_value_type_float(const char * name, measurements_data_t* data)
{
    if (data->num_samples == 1)
        return _protocol_append_data_type_float(name, data->value.value_f.sum);
    bool r = true;
    int32_t mean = data->value.value_f.sum / data->num_samples;
    char tmp[MEASURE_NAME_NULLED_LEN + 4];
    r &= _protocol_append_data_type_float(name, mean);
    snprintf(tmp, sizeof(tmp), "%s_min", name);
    r &= _protocol_append_data_type_float(tmp, data->value.value_f.min);
    snprintf(tmp, sizeof(tmp), "%s_max", name);
    r &= _protocol_append_data_type_float(tmp, data->value.value_f.max);
    return r;
}


static bool _protocol_append_value_type_i64(const char * name, measurements_data_t* data)
{
    if (data->num_samples == 1)
        return _protocol_append_data_type_i64(name, data->value.value_f.sum);
    bool r = true;
    int64_t mean = data->value.value_64.sum / data->num_samples;
    char tmp[MEASURE_NAME_NULLED_LEN + 4];
    r &= _protocol_append_data_type_i64(name, mean);
    snprintf(tmp, sizeof(tmp), "%s_min", name);
    r &= _protocol_append_data_type_i64(tmp, data->value.value_64.min);
    snprintf(tmp, sizeof(tmp), "%s_max", name);
    r &= _protocol_append_data_type_i64(tmp, data->value.value_64.max);
    return r;
}



static bool _protocol_append_value_type_str(const char * name, measurements_data_t* data)
{
    return _protocol_append_meas("\"%s\":\"%s\"", name, data->value.value_s.str);
}


bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data)
{
    char * name = def->name;
    unsigned before_pos = _json_buf_pos;
    bool ret = false;
    switch(data->value_type)
    {
        case MEASUREMENTS_VALUE_TYPE_I64:
            ret = _protocol_append_value_type_i64(name, data);
            break;
        case MEASUREMENTS_VALUE_TYPE_STR:
            ret = _protocol_append_value_type_str(name, data);
            break;
        case MEASUREMENTS_VALUE_TYPE_FLOAT:
            ret = _protocol_append_value_type_float(name, data );
            break;
        default:
            log_error("Unknown type '%"PRIu8"'.", data->value_type);
            break;
    }
    if (!ret)
    {
        log_sys_debug("Early exit %u -> %u", _json_buf_pos, before_pos);
        _json_buf_pos = before_pos;
    }
    return ret;
}


bool protocol_send(void)
{
    unsigned available = JSON_BUF_SIZE - _json_buf_pos;

    if (available < JSON_CLOSE_SIZE)
    {
        comms_debug("Space error");
        return false;
    }
    log_sys_debug("_json_buf_pos = %u", _json_buf_pos);
    log_sys_debug("_json_buf(%u) = %s", _json_buf_pos, _json_buf);

    unsigned r = snprintf(_json_buf + _json_buf_pos, available, "}}");

    _json_buf_pos += r;

    log_sys_debug("_json_buf(%u) = %s", _json_buf_pos, _json_buf);

    if (!comms_send(_json_buf, _json_buf_pos))
    {
        return false;
    }
    comms_debug("batch complete.");
    return true;
}


bool protocol_init(void)
{
    int64_t ts;

    if (!comms_get_unix_time(&ts))
        return false;

    _json_buf_pos = 0;

    return _protocol_append("{\"UNIX\":%"PRIi64",\"NAME\":\"%.*s\",\"VALUES\":{", ts, HUMAN_NAME_LEN, persist_get_human_name());
}
