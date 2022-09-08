#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "rak3172.h"

#include "lw.h"
#include "common.h"
#include "log.h"
#include "base_types.h"
#include "uart_rings.h"


#define RAK3172_TIMEOUT_MS              15000
#define RAK3172_JOIN_TIME_S             ((RAK3172_TIMEOUT_MS/1000) - 5)

_Static_assert(RAK3172_JOIN_TIME_S > 5, "RAK3172 join time is less than 5");

#define RAK3172_PAYLOAD_MAX_DEFAULT     242

#define RAK3172_SHORT_RESET_COUNT       5
#define RAK3172_SHORT_RESET_TIME_MS     10
#define RAK3172_LONG_RESET_TIME_MS      (15 * 60 * 1000)

#define RAK3172_MAX_CMD_LEN             64
#define RAK3172_INIT_MSG_LEN            64

#define RAK3172_MSG_INIT                "INITIALIZATION OK"
#define RAK3172_MSG_OK                  "OK"
#define RAK3172_MSG_JOIN                "AT+JOIN=1:0:"STR(RAK3172_JOIN_TIME_S)":0"
#define RAK3172_MSG_JOINED              "+EVT:JOINED"
#define RAK3172_MSG_SEND                "AT+SEND="


typedef enum
{
    RAK3172_STATE_OFF,
    RAK3172_STATE_INIT_WAIT_OK,
    RAK3172_STATE_INIT_WAIT_REPLAY,
    RAK3172_STATE_JOIN_WAIT_OK,
    RAK3172_STATE_JOIN_WAIT_REPLAY,
    RAK3172_STATE_JOIN_WAIT_JOIN,
    RAK3172_STATE_RESETTING,
    RAK3172_STATE_IDLE,
    RAK3172_STATE_SEND_WAIT_OK,
    RAK3172_STATE_SEND_WAIT_ACK,
} rak3172_state_t;


static uint16_t _rak3172_packet_max_size = RAK3172_PAYLOAD_MAX_DEFAULT;


struct
{
    uint8_t             init_count;
    uint8_t             reset_count;
    uint8_t             port;
    rak3172_state_t     state;
    uint32_t            cmd_last_sent;
    uint32_t            wakeup_time;
    port_n_pins_t       reset_pin;
} _rak3172_ctx = {.init_count       = 0,
                  .reset_count      = 0,
                  .state            = RAK3172_STATE_OFF,
                  .cmd_last_sent    = 0,
                  .wakeup_time      = 0,
                  .reset_pin        = { GPIOC, GPIO8 }};


const char _rak3172_init_msgs[][RAK3172_INIT_MSG_LEN] = {
        "ATE",                  /* Enable line replay */
        "AT+NWM=1",             /* Set LoRaWAN mode   */
        "AT+NJM=1",             /* Set OTAA mode      */
        "AT+CLASS=A",           /* Set Class A mode   */
        "AT+BAND=4",            /* Set to EU868       */
        "DEVEUI goes here",
        "APPEUI goes here",
        "APPKEY goes here"};


static uint8_t _rak3172_get_port(void)
{
    _rak3172_ctx.port++;
    if (_rak3172_ctx.port > 223)
        _rak3172_ctx.port = 0;
    return _rak3172_ctx.port;
}


static void _rak3172_chip_on(void)
{
    gpio_set(_rak3172_ctx.reset_pin.port, _rak3172_ctx.reset_pin.pins);
}


static void _rak3172_chip_off(void)
{
    gpio_clear(_rak3172_ctx.reset_pin.port, _rak3172_ctx.reset_pin.pins);
}


static bool _rak3172_write(char* cmd)
{
    return (bool)uart_ring_out(COMMS_UART, cmd, strlen(cmd));
}


static int _rak3172_printf(char* fmt, ...)
{
    char buf[RAK3172_MAX_CMD_LEN];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, RAK3172_MAX_CMD_LEN - 3, fmt, args);
    va_end(args);
    comms_debug("<< %s", buf);
    buf[len] = '\r';
    buf[len+1] = '\n';
    buf[len+2] = 0;
    return _rak3172_write(buf) ? len : 0;
}


static void _rak3172_process_state_off(char* msg)
{
    if (msg_is(RAK3172_MSG_INIT, msg))
    {
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
        _rak3172_ctx.init_count = 0;
        _rak3172_printf((char*)_rak3172_init_msgs[_rak3172_ctx.init_count++]);
    }
}


static void _rak3172_process_state_init_wait_ok(char* msg)
{
    if (msg_is(RAK3172_MSG_OK, msg))
    {
        if (_rak3172_ctx.init_count == 0)
            return;
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_REPLAY;
    }
}


static void _rak3172_process_state_init_wait_replay(char* msg)
{
    if (msg_is(_rak3172_init_msgs[_rak3172_ctx.init_count], msg))
    {
        if (ARRAY_SIZE(_rak3172_init_msgs) == _rak3172_ctx.init_count + 1)
        {
            _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_OK;
            _rak3172_printf(RAK3172_MSG_JOIN);
            return;
        }
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
        _rak3172_printf((char*)_rak3172_init_msgs[_rak3172_ctx.init_count++]);
    }
}


static void _rak3172_process_state_join_wait_ok(char* msg)
{
    if (msg_is(RAK3172_MSG_OK, msg))
        _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_REPLAY;
}


static void _rak3172_process_state_join_wait_replay(char* msg)
{
    if (msg_is(RAK3172_MSG_JOIN, msg))
        _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_JOIN;
}


static void _rak3172_process_state_join_wait_join(char* msg)
{
    if (msg_is(RAK3172_MSG_JOINED, msg))
        _rak3172_ctx.state = RAK3172_STATE_IDLE;
}


uint16_t rak3172_get_mtu(void)
{
    return _rak3172_packet_max_size;
}


bool rak3172_send_ready(void)
{
    return (_rak3172_ctx.state == RAK3172_STATE_IDLE);
}


bool rak3172_send_str(char* str)
{
    if (!rak3172_send_ready())
    {
        comms_debug("Cannot send '%s' as chip is not in IDLE state.", str);
        return false;
    }
    _rak3172_ctx.state = RAK3172_STATE_SEND_WAIT_OK;
    _rak3172_printf("AT+SEND=:%u:%s", _rak3172_get_port(), str);
    return true;
}


bool rak3172_send_allowed(void)
{
    return false;
}


void rak3172_init(void)
{
    _rak3172_ctx.state = RAK3172_STATE_OFF;
    _rak3172_chip_on();
}


void rak3172_reset(void)
{
    if (_rak3172_ctx.state == RAK3172_STATE_RESETTING)
        return;
    _rak3172_ctx.state = RAK3172_STATE_RESETTING;
    uint32_t wakeup_time = get_since_boot_ms();
    if (_rak3172_ctx.reset_count <= RAK3172_SHORT_RESET_COUNT)
    {
        wakeup_time += RAK3172_SHORT_RESET_TIME_MS;
        _rak3172_ctx.reset_count++;
    }
    else
        wakeup_time += RAK3172_LONG_RESET_TIME_MS;
    _rak3172_ctx.wakeup_time = wakeup_time;
    _rak3172_chip_off();
}


static bool _rak3172_reload_config(void)
{
    return true;
}


void rak3172_process(char* msg)
{
    switch (_rak3172_ctx.state)
    {
        case RAK3172_STATE_OFF:
            _rak3172_process_state_off(msg);
            break;
        case RAK3172_STATE_INIT_WAIT_OK:
            _rak3172_process_state_init_wait_ok(msg);
            break;
        case RAK3172_STATE_INIT_WAIT_REPLAY:
            _rak3172_process_state_init_wait_replay(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_OK:
            _rak3172_process_state_join_wait_ok(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_REPLAY:
            _rak3172_process_state_join_wait_replay(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_JOIN:
            _rak3172_process_state_join_wait_join(msg);
            break;
        default:
            comms_debug("Unknown state.");
            return;
    }
}


void rak3172_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_rak3172_ctx.state != RAK3172_STATE_IDLE)
    {
        comms_debug("Incorrect state to send : %u", (unsigned)_rak3172_ctx.state);
        return;
    }

    if (!_rak3172_write(RAK3172_MSG_SEND))
    {
        comms_debug("Could not write SEND header.");
        return;
    }
    char hex_str[5];
    snprintf(hex_str, 4, "%"PRIu8":", _rak3172_get_port());
    if (!_rak3172_write(hex_str))
    {
        comms_debug("Could not write the port number.");
        return;
    }

    memset(hex_str, 0, 5);
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(hex_str, 3, "%02"PRIx8, hex_arr[i]);
        _rak3172_write(hex_str);
        uart_ring_out(CMD_UART, hex_str, 2);
    }
    _rak3172_write("\r\n");
    uart_ring_out(CMD_UART, "\r\n", 2);
    _rak3172_ctx.state = RAK3172_STATE_SEND_WAIT_OK;
}


bool rak3172_get_connected(void)
{
    return (_rak3172_ctx.state == RAK3172_STATE_IDLE);
}


void rak3172_loop_iteration(void)
{
    switch(_rak3172_ctx.state)
    {
        case RAK3172_STATE_OFF:
            break;
        case RAK3172_STATE_IDLE:
            break;
        case RAK3172_STATE_RESETTING:
            if (since_boot_delta(get_since_boot_ms(), _rak3172_ctx.wakeup_time))
                _rak3172_chip_on();
            break;
        default:
            if (since_boot_delta(get_since_boot_ms(), _rak3172_ctx.cmd_last_sent) > RAK3172_TIMEOUT_MS)
                rak3172_reset();
            break;
    }
}


void rak3172_config_setup_str(char* str)
{
    if (lw_config_setup_str(str))
        _rak3172_reload_config();
}
