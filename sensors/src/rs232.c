#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "measurements.h"
#include "ring.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "common.h"


#define RS232_PROMPT_CHAR               'A'
#define RS232_EXPECT_CHAR               'B'
#define RS232_MSG_LEN                   2


static struct
{
    struct
    {
        char        msg[RS232_MSG_LEN];
        unsigned    len;
        uint32_t    time;
    } stored_msg;
    bool has_errored;
} _rs232_ctx =
{
    .stored_msg =
    {
        .msg = { 0 },
        .len = 0,
        .time = 0,
    },
    .has_errored = false,
};


static unsigned _rs232_send(char* msg, unsigned len)
{
    return uart_ring_out(RS232_UART, msg, len);
}


static bool _rs232_process(ring_buf_t* ring, char* buf, unsigned buflen, unsigned* len)
{
    unsigned len_l = ring_buf_get_pending(ring);
    if (!len_l)
    {
        /* nothing to read */
        return false;
    }
    if (len_l > buflen - 1)
    {
        len_l = buflen - 1;
    }
    len_l = ring_buf_read(ring, buf, len_l);
    buf[len_l] = 0;
    *len = len_l;
    if (!len_l)
    {
        /* there was something to read, it has either gone, or couldn't be read */
        return false;
    }
    return true;
}


void rs232_process(ring_buf_t* ring)
{
    unsigned len = 0;
    char buf[RS232_MSG_LEN];
    bool did_success = _rs232_process(ring, buf, RS232_MSG_LEN, &len);
    if (did_success && !_rs232_ctx.has_errored)
    {
        _rs232_ctx.has_errored = !(!_rs232_ctx.has_errored && _rs232_ctx.stored_msg.time == 0 && _rs232_ctx.stored_msg.len == 0 && *_rs232_ctx.stored_msg.msg == 0);
        if (len > RS232_MSG_LEN - 1)
        {
            len = RS232_MSG_LEN - 1;
        }
        memcpy(_rs232_ctx.stored_msg.msg, buf, len);
        _rs232_ctx.stored_msg.len = len;
        _rs232_ctx.stored_msg.time = get_since_boot_ms();
    }
}


static measurements_sensor_state_t _rs232_start(char* name, bool in_isolation)
{
    char c = RS232_PROMPT_CHAR;
    return _rs232_send(&c, 1) ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _rs232_collect(char* name, measurements_reading_t* value)
{
    static char msg[RS232_MSG_LEN];
    if (_rs232_ctx.has_errored || !_rs232_ctx.stored_msg.len || !_rs232_ctx.stored_msg.time || !*_rs232_ctx.stored_msg.msg)
    {
        _rs232_ctx.has_errored = false;
        _rs232_ctx.stored_msg.msg[0] = 0;
        _rs232_ctx.stored_msg.len = 0;
        _rs232_ctx.stored_msg.time = 0;
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    memcpy(msg, _rs232_ctx.stored_msg.msg, _rs232_ctx.stored_msg.len);
    msg[_rs232_ctx.stored_msg.len] = 0;
    value->v_str = msg;
    _rs232_ctx.stored_msg.msg[0] = 0;
    _rs232_ctx.stored_msg.len = 0;
    _rs232_ctx.stored_msg.time = 0;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _rs232_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_STR;
}


static measurements_sensor_state_t _rs232_get_collection_time(char* name, uint32_t* collection_time)
{
    *collection_time = 1000;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void rs232_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _rs232_get_collection_time;
    inf->init_cb            = _rs232_start;
    inf->get_cb             = _rs232_collect;
    inf->value_type_cb      = _rs232_value_type;
}
