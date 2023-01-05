#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "rak3172.h"

#include "lw.h"
#include "common.h"
#include "log.h"
#include "base_types.h"
#include "uart_rings.h"
#include "pinmap.h"


#define RAK3172_TIMEOUT_MS              15000
#define RAK3172_ACK_TIMEOUT_MS          20000
#define RAK3172_JOIN_TIME_S             (uint32_t)((RAK3172_TIMEOUT_MS/1000) - 5)

_Static_assert(RAK3172_JOIN_TIME_S > 5, "RAK3172 join time is less than 5");

#define RAK3172_PAYLOAD_MAX_DEFAULT     242

#define RAK3172_SHORT_RESET_COUNT       5
#define RAK3172_SHORT_RESET_TIME_MS     10
#define RAK3172_LONG_RESET_TIME_MS      (15 * 60 * 1000)

#define RAK3172_MAX_CMD_LEN             64
#define RAK3172_INIT_MSG_LEN            64

#define RAK3172_MSG_INIT                "Current Work Mode: LoRaWAN."
#define RAK3172_MSG_OK                  "OK"
#define RAK3172_MSG_JOIN                "AT+JOIN=1:0:%"PRIu32":0"
#define RAK3172_MSG_JOINED              "+EVT:JOINED"
#define RAK3172_MSG_JOIN_FAILED         "+EVT:JOIN FAILED"
#define RAK3172_MSG_USEND_HEADER_FMT    "AT+USEND=%"PRIu8":%"PRIu8":%"PRIu8":"
#define RAK3172_MSG_USEND_HEADER_LEN    (9 + (1 * 3) + (3 * 3) + 1)          /* strlen("AT+USEND=:") + (strlen(":") * 3) + (strlen("255") * 3) + 1 */
#define RAK3172_MSG_ACK                 "+EVT:SEND CONFIRMED OK"


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
    RAK3172_STATE_SEND_WAIT_REPLAY,
    RAK3172_STATE_SEND_WAIT_OK,
    RAK3172_STATE_SEND_WAIT_ACK,
} rak3172_state_t;


static uint16_t _rak3172_packet_max_size = RAK3172_PAYLOAD_MAX_DEFAULT;


struct
{
    uint8_t             init_count;
    uint8_t             reset_count;
    rak3172_state_t     state;
    uint32_t            cmd_last_sent;
    uint32_t            wakeup_time;
    port_n_pins_t       reset_pin;
    port_n_pins_t       boot_pin;
    bool                config_is_valid;
    char                last_sent_msg[RAK3172_MAX_CMD_LEN+1];
} _rak3172_ctx = {.init_count       = 0,
                  .reset_count      = 0,
                  .state            = RAK3172_STATE_OFF,
                  .cmd_last_sent    = 0,
                  .wakeup_time      = 0,
                  .reset_pin        = REV_C_COMMS_RESET_PORT_N_PINS,
                  .boot_pin         = REV_C_COMMS_BOOT_PORT_N_PINS,
                  .config_is_valid  = false,
                  .last_sent_msg    = {0}};


char _rak3172_init_msgs[][RAK3172_INIT_MSG_LEN] = {
        "ATE",                  /* Enable line replay */
        "AT+CFM=1",             /* Set confirmation   */
        "AT+NWM=1",             /* Set LoRaWAN mode   */
        "AT+NJM=1",             /* Set OTAA mode      */
        "AT+CLASS=C",           /* Set Class A mode   */
        "AT+BAND=4",            /* Set to EU868       */
        "AT+DR=4",              /* Set to DR 4        */
        "DEVEUI goes here",
        "APPEUI goes here",
        "APPKEY goes here"};


static uint8_t _rak3172_get_port(void)
{
    return 1;
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
    unsigned len = strnlen(cmd, RAK3172_MAX_CMD_LEN);
    return (bool)uart_ring_out(COMMS_UART, cmd, len);
}


static int _rak3172_printf(char* fmt, ...)
{
    char buf[RAK3172_MAX_CMD_LEN];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, RAK3172_MAX_CMD_LEN - 3, fmt, args);
    va_end(args);
    _rak3172_ctx.cmd_last_sent = get_since_boot_ms();
    memcpy(_rak3172_ctx.last_sent_msg, buf, len);
    _rak3172_ctx.last_sent_msg[len] = 0;
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = '\n';
    buf[len+2] = 0;
    return _rak3172_write(buf) ? len : 0;
}


static void _rak3172_process_state_off(char* msg)
{
    if (msg_is(RAK3172_MSG_INIT, msg))
    {
        comms_debug("READ INIT MESSAGE");
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
        _rak3172_ctx.init_count = 0;
        _rak3172_printf((char*)_rak3172_init_msgs[_rak3172_ctx.init_count]);
    }
}


static void _rak3172_process_state_init_wait_replay(char* msg)
{
    if (msg_is(_rak3172_init_msgs[_rak3172_ctx.init_count], msg))
    {
        comms_debug("READ INIT REPLAY");
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
    }
}


static void _rak3172_send_join(void)
{
    char join_msg[RAK3172_MAX_CMD_LEN+1];
    snprintf(join_msg, RAK3172_MAX_CMD_LEN, RAK3172_MSG_JOIN, RAK3172_JOIN_TIME_S);
    _rak3172_printf(join_msg);
}


static bool _rak3172_msg_is_replay(char* msg)
{
    unsigned len = strnlen(msg, RAK3172_MAX_CMD_LEN);
    unsigned sent_len = strnlen(_rak3172_ctx.last_sent_msg, RAK3172_MAX_CMD_LEN);
    comms_debug("LAST SENT: %s", _rak3172_ctx.last_sent_msg);
    if (len != sent_len)
        return false;
    return (strncmp(msg, _rak3172_ctx.last_sent_msg, len) == 0);
}


static void _rak3172_process_state_init_wait_ok(char* msg)
{
    if (msg_is(RAK3172_MSG_OK, msg))
    {
        comms_debug("READ INIT OK");
        if (ARRAY_SIZE(_rak3172_init_msgs) == _rak3172_ctx.init_count + 1)
        {
            comms_debug("FINISHED INIT");
            _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_REPLAY;
            _rak3172_send_join();
            return;
        }
        comms_debug("SENDING NEXT INIT");
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_REPLAY;
        _rak3172_printf((char*)_rak3172_init_msgs[++_rak3172_ctx.init_count]);
    }
}


static void _rak3172_process_state_join_wait_replay(char* msg)
{
    if (_rak3172_msg_is_replay(msg))
    {
        comms_debug("READ JOIN REPLAY");
        _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_OK;
    }
    else
    {
        comms_debug("UNKNOWN: %s", msg);
    }
}


static void _rak3172_process_state_join_wait_ok(char* msg)
{
    if (msg_is(RAK3172_MSG_OK, msg))
    {
        comms_debug("READ JOIN OK");
        _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_JOIN;
    }
}


static void _rak3172_process_state_join_wait_join(char* msg)
{
    if (msg_is(RAK3172_MSG_JOINED, msg))
    {
        comms_debug("READ JOIN");
        _rak3172_ctx.state = RAK3172_STATE_IDLE;
    }
    else if (msg_is(RAK3172_MSG_JOIN_FAILED, msg))
    {
        comms_debug("READ JOIN FAILED");
        _rak3172_ctx.state = RAK3172_STATE_JOIN_WAIT_REPLAY;
        _rak3172_send_join();
    }
}


static bool _rak3172_reload_config(void)
{
    return true;
}


static void _rak3172_process_state_send_replay(char* msg)
{
    if (_rak3172_msg_is_replay(msg))
    {
        comms_debug("READ SEND REPLAY");
        _rak3172_ctx.state = RAK3172_STATE_SEND_WAIT_OK;
    }
}


static void _rak3172_process_state_send_ok(char* msg)
{
    if (msg_is(RAK3172_MSG_OK, msg))
    {
        comms_debug("READ SEND OKAY");
        _rak3172_ctx.state = RAK3172_STATE_SEND_WAIT_ACK;
    }
}


static void _rak3172_process_state_send_ack(char* msg)
{
    if (msg_is(RAK3172_MSG_ACK, msg))
    {
        comms_debug("READ SEND ACK");
        _rak3172_ctx.state = RAK3172_STATE_IDLE;
        on_comms_sent_ack(true);
    }
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
    _rak3172_ctx.state = RAK3172_STATE_SEND_WAIT_REPLAY;
    _rak3172_printf("AT+SEND=:%u:%s", _rak3172_get_port(), str);
    return true;
}


bool rak3172_send_allowed(void)
{
    return false;
}


static bool _rak3172_load_config(void)
{
    if (!lw_persist_data_is_valid())
    {
        log_error("No LoRaWAN Dev EUI and/or App Key.");
        return false;
    }

    lw_config_t* config = lw_get_config();
    if (!config)
        return false;

    snprintf(_rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-3], RAK3172_INIT_MSG_LEN, "AT+DEVEUI=%."STR(LW_DEV_EUI_LEN)"s", config->dev_eui);
    snprintf(_rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-2], RAK3172_INIT_MSG_LEN, "AT+APPEUI=%."STR(LW_DEV_EUI_LEN)"s", config->dev_eui);
    snprintf(_rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-1], RAK3172_INIT_MSG_LEN, "AT+APPKEY=%."STR(LW_APP_KEY_LEN)"s", config->app_key);
    return true;
}


void rak3172_init(void)
{
    if (!_rak3172_load_config())
    {
        comms_debug("Config is incorrect, not initialising.");
        return;
    }

    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.reset_pin.port));
    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.boot_pin.port));
    gpio_mode_setup(_rak3172_ctx.reset_pin.port, false, GPIO_PUPD_NONE, _rak3172_ctx.reset_pin.pins);
    gpio_mode_setup(_rak3172_ctx.boot_pin.port,  false, GPIO_PUPD_NONE, _rak3172_ctx.boot_pin.pins );

    _rak3172_ctx.state = RAK3172_STATE_OFF;
    _rak3172_chip_on();
}


void rak3172_reset(void)
{
    comms_debug("CALLED RESET");
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


void rak3172_process(char* msg)
{
    switch (_rak3172_ctx.state)
    {
        case RAK3172_STATE_OFF:
            _rak3172_process_state_off(msg);
            break;
        case RAK3172_STATE_INIT_WAIT_REPLAY:
            _rak3172_process_state_init_wait_replay(msg);
            break;
        case RAK3172_STATE_INIT_WAIT_OK:
            _rak3172_process_state_init_wait_ok(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_REPLAY:
            _rak3172_process_state_join_wait_replay(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_OK:
            _rak3172_process_state_join_wait_ok(msg);
            break;
        case RAK3172_STATE_JOIN_WAIT_JOIN:
            _rak3172_process_state_join_wait_join(msg);
            break;
        case RAK3172_STATE_RESETTING:
            break;
        case RAK3172_STATE_IDLE:
            break;
        case RAK3172_STATE_SEND_WAIT_REPLAY:
            _rak3172_process_state_send_replay(msg);
            break;
        case RAK3172_STATE_SEND_WAIT_OK:
            _rak3172_process_state_send_ok(msg);
            break;
        case RAK3172_STATE_SEND_WAIT_ACK:
            _rak3172_process_state_send_ack(msg);
            break;
        default:
            comms_debug("Unknown state. (%d)", _rak3172_ctx.state);
            return;
    }
}


#define RAK3172_NB_TRIALS               2


void rak3172_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_rak3172_ctx.state != RAK3172_STATE_IDLE)
    {
        comms_debug("Incorrect state to send : %u", (unsigned)_rak3172_ctx.state);
        return;
    }

    char send_header[RAK3172_MSG_USEND_HEADER_LEN];

    bool confirmed_payload = true;

    snprintf(send_header, RAK3172_MAX_CMD_LEN, RAK3172_MSG_USEND_HEADER_FMT, _rak3172_get_port(), (uint8_t)confirmed_payload, RAK3172_NB_TRIALS);

    if (!_rak3172_write(send_header))
    {
        comms_debug("Could not write SEND header.");
        return;
    }
    char hex_str[5];
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
    _rak3172_ctx.cmd_last_sent = get_since_boot_ms();
}


bool rak3172_get_connected(void)
{
    return (_rak3172_ctx.state == RAK3172_STATE_IDLE                ||
            _rak3172_ctx.state == RAK3172_STATE_SEND_WAIT_REPLAY    ||
            _rak3172_ctx.state == RAK3172_STATE_SEND_WAIT_OK        ||
            _rak3172_ctx.state == RAK3172_STATE_SEND_WAIT_ACK       );
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
        case RAK3172_STATE_SEND_WAIT_ACK:
            if (since_boot_delta(get_since_boot_ms(), _rak3172_ctx.cmd_last_sent) > RAK3172_ACK_TIMEOUT_MS)
            {
                comms_debug("TIMED OUT WAITING FOR ACK");
                on_comms_sent_ack(false);
                rak3172_reset();
            }
            break;
        default:
            if (since_boot_delta(get_since_boot_ms(), _rak3172_ctx.cmd_last_sent) > RAK3172_TIMEOUT_MS)
            {
                comms_debug("TIMED OUT");
                rak3172_reset();
            }
            break;
    }
}


void rak3172_config_setup_str(char* str)
{
    if (lw_config_setup_str(str))
        _rak3172_reload_config();
}


static bool _rak3172_boot_enabled   = false;
static bool _rak3172_reset_enabled  = true;


static void _rak3172_print_boot_reset_cb(char* args)
{
    log_out("BOOT = %"PRIu8, (uint8_t)_rak3172_boot_enabled);
    log_out("RESET = %"PRIu8, (uint8_t)_rak3172_reset_enabled);
}


static void _rak3172_boot_cb(char* args)
{
    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.boot_pin.port));
    gpio_mode_setup(_rak3172_ctx.boot_pin.port,  false, GPIO_PUPD_NONE, _rak3172_ctx.boot_pin.pins );

    bool enabled = strtoul(args, NULL, 10);
    if (enabled)
        gpio_set(_rak3172_ctx.boot_pin.port, _rak3172_ctx.boot_pin.pins);
    else
        gpio_clear(_rak3172_ctx.boot_pin.port, _rak3172_ctx.boot_pin.pins);
    _rak3172_boot_enabled = enabled;
    _rak3172_print_boot_reset_cb("");
}


static void _rak3172_reset_cb(char* args)
{
    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.reset_pin.port));
    gpio_mode_setup(_rak3172_ctx.reset_pin.port, false, GPIO_PUPD_NONE, _rak3172_ctx.reset_pin.pins);

    bool enabled = strtoul(args, NULL, 10);
    if (enabled)
        gpio_set(_rak3172_ctx.reset_pin.port, _rak3172_ctx.reset_pin.pins);
    else
        gpio_clear(_rak3172_ctx.reset_pin.port, _rak3172_ctx.reset_pin.pins);
    _rak3172_reset_enabled = enabled;
    _rak3172_print_boot_reset_cb("");
}


static void _rak3172_state_cb(char* args)
{
    log_out("STATE: %d", _rak3172_ctx.state);
    log_out("COUNT: %"PRIu8, _rak3172_ctx.init_count);
}


struct cmd_link_t* rak3172_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "comms_print",  "Print boot/reset line",       _rak3172_print_boot_reset_cb  , false , NULL },
                                       { "comms_boot",   "Enable/disable boot line",    _rak3172_boot_cb              , false , NULL },
                                       { "comms_reset",  "Enable/disable reset line",   _rak3172_reset_cb             , false , NULL },
                                       { "comms_state",  "Print comms state",           _rak3172_state_cb             , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
