#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <osm/core/measurements.h>
#include <osm/core/ring.h>
#include <osm/core/uart_rings.h>
#include "pinmap.h"
#include <osm/core/common.h>


#define EXAMPLE_RS232_PROMPT_CHAR               'A'
#define EXAMPLE_RS232_EXPECT_CHAR               'B'
#define EXAMPLE_RS232_MSG_LEN                   2


static struct
{
    struct
    {
        char        msg[EXAMPLE_RS232_MSG_LEN];
        unsigned    len;
        uint32_t    time;
    } stored_msg;
    bool has_errored;
} _example_rs232_ctx =
{
    .stored_msg =
    {
        .msg = { 0 },
        .len = 0,
        .time = 0,
    },
    .has_errored = false,
};


static unsigned _example_rs232_send(char* msg, unsigned len)
{
    return uart_ring_out(RS232_UART, msg, len);
}


static bool _example_rs232_process(ring_buf_t* ring, char* buf, unsigned buflen, unsigned* len)
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


void example_rs232_process(ring_buf_t* ring)
{
    unsigned len = 0;
    char buf[EXAMPLE_RS232_MSG_LEN];
    bool did_success = _example_rs232_process(ring, buf, EXAMPLE_RS232_MSG_LEN, &len);
    if (did_success && !_example_rs232_ctx.has_errored)
    {
        _example_rs232_ctx.has_errored = !(!_example_rs232_ctx.has_errored && _example_rs232_ctx.stored_msg.time == 0 && _example_rs232_ctx.stored_msg.len == 0 && *_example_rs232_ctx.stored_msg.msg == 0);
        if (len > EXAMPLE_RS232_MSG_LEN - 1)
        {
            len = EXAMPLE_RS232_MSG_LEN - 1;
        }
        memcpy(_example_rs232_ctx.stored_msg.msg, buf, len);
        _example_rs232_ctx.stored_msg.len = len;
        _example_rs232_ctx.stored_msg.time = get_since_boot_ms();
    }
}


static measurements_sensor_state_t _example_rs232_start(char* name, bool in_isolation)
{
    char c = EXAMPLE_RS232_PROMPT_CHAR;
    return _example_rs232_send(&c, 1) ? MEASUREMENTS_SENSOR_STATE_SUCCESS : MEASUREMENTS_SENSOR_STATE_ERROR;
}


static measurements_sensor_state_t _example_rs232_collect(char* name, measurements_reading_t* value)
{
    static char msg[EXAMPLE_RS232_MSG_LEN];
    if (_example_rs232_ctx.has_errored || !_example_rs232_ctx.stored_msg.len || !_example_rs232_ctx.stored_msg.time || !*_example_rs232_ctx.stored_msg.msg)
    {
        _example_rs232_ctx.has_errored = false;
        _example_rs232_ctx.stored_msg.msg[0] = 0;
        _example_rs232_ctx.stored_msg.len = 0;
        _example_rs232_ctx.stored_msg.time = 0;
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    memcpy(msg, _example_rs232_ctx.stored_msg.msg, _example_rs232_ctx.stored_msg.len);
    msg[_example_rs232_ctx.stored_msg.len] = 0;
    value->v_str = msg;
    _example_rs232_ctx.stored_msg.msg[0] = 0;
    _example_rs232_ctx.stored_msg.len = 0;
    _example_rs232_ctx.stored_msg.time = 0;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _example_rs232_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_STR;
}


static measurements_sensor_state_t _example_rs232_get_collection_time(char* name, uint32_t* collection_time)
{
    *collection_time = 1000;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


void example_rs232_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _example_rs232_get_collection_time;
    inf->init_cb            = _example_rs232_start;
    inf->get_cb             = _example_rs232_collect;
    inf->value_type_cb      = _example_rs232_value_type;
}
