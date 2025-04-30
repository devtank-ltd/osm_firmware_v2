#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include <osm/comms/rak3172.h>

#include <osm/comms/lw.h>
#include <osm/core/common.h>
#include <osm/core/log.h>
#include <osm/core/base_types.h>
#include <osm/core/uart_rings.h>
#include "pinmap.h"
#include <osm/core/sleep.h>
#include <osm/core/cmd.h>
#include <osm/core/update.h>
#include <osm/protocols/protocol.h>


#define RAK3172_TIMEOUT_MS              15000
#define RAK3172_ACK_TIMEOUT_MS          20000
#define RAK3172_JOIN_TIME_S             (uint32_t)((RAK3172_TIMEOUT_MS/1000) - 5)

#define RAK3172_SEND_DELAY_MS           2000

_Static_assert(RAK3172_JOIN_TIME_S > 5, "RAK3172 join time is less than 5");

#define RAK3172_NB_TRIALS               2
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
#define RAK3172_MSG_JOIN_FAILED         "+EVT:JOIN_FAILED_RX_TIMEOUT"
#define RAK3172_MSG_SEND_HEADER_FMT     "AT+SEND=%"PRIu8":"
#define RAK3172_MSG_SEND_HEADER_LEN     13
#define RAK3172_MSG_ACK                 "+EVT:SEND_CONFIRMED_OK"
#define RAK3172_MSG_NACK                "+EVT:SEND_CONFIRMED_FAILED"


typedef enum
{
    RAK3172_STATE_OFF = 0,
    RAK3172_STATE_INIT_WAIT_BOOT,
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


static uint16_t _rak3172_packet_max_size        = RAK3172_PAYLOAD_MAX_DEFAULT;

static bool     _rak3172_boot_enabled           = false;
static bool     _rak3172_reset_enabled          = true;
static uint16_t _rak3172_next_fw_chunk_id       = 0;
static char     _rak3172_ascii_cmd[CMD_LINELEN] = {0};


struct
{
    uint8_t             init_count;
    uint32_t            reset_count;
    rak3172_state_t     state;
    uint32_t            cmd_last_sent;
    uint32_t            sleep_from_time;
    port_n_pins_t       reset_pin;
    port_n_pins_t       boot_pin;
    bool                config_is_valid;
    char                last_sent_msg[RAK3172_MAX_CMD_LEN+1];
    uint8_t             err_code;
} _rak3172_ctx =
{
    .init_count       = 0,
    .reset_count      = 0,
    .state            = RAK3172_STATE_OFF,
    .cmd_last_sent    = 0,
    .sleep_from_time  = 0,
    .reset_pin        = COMMS_RESET_PORT_N_PINS,
    .boot_pin         = COMMS_BOOT_PORT_N_PINS,
    .config_is_valid  = false,
    .last_sent_msg    = {0},
    .err_code         = 0,
};


char _rak3172_init_msgs[][RAK3172_INIT_MSG_LEN] =
{
    "ATR",                  /* Reset config       */
    "AT+NWM=1",             /* Set LoRaWAN mode   */
    "ATE",                  /* Enable line replay */
    "AT+CFM=1",             /* Set confirmation   */
    "AT+NJM=1",             /* Set OTAA mode      */
    "AT+CLASS=C",           /* Set Class A mode   */
    "AT+ADR=0",             /* Do not use ADR     */
    "AT+DR=4",              /* Set to DR 4        */
    "AT+TXP=0",             /* Set highest TX     */
    "REGION goes here",     /* Set to EU868       */
    "DEVEUI goes here",
    "APPEUI goes here",
    "APPKEY goes here"
};


static uint8_t _rak3172_get_port(void)
{
    return 1;
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


static void _rak3172_reset_line_set(bool enabled)
{
    if (enabled)
    {
        gpio_mode_setup(_rak3172_ctx.reset_pin.port,
                        GPIO_MODE_INPUT,
                        GPIO_PUPD_NONE,
                        _rak3172_ctx.reset_pin.pins);
    }
    else
    {
        gpio_mode_setup(_rak3172_ctx.reset_pin.port,
                        GPIO_MODE_OUTPUT,
                        GPIO_PUPD_NONE,
                        _rak3172_ctx.reset_pin.pins);
        gpio_clear(_rak3172_ctx.reset_pin.port, _rak3172_ctx.reset_pin.pins);
    }
    _rak3172_reset_enabled = enabled;
}


static void _rak3172_hard_reset(void)
{
    comms_debug("HARD RESET");
    _rak3172_reset_line_set(false);
    spin_blocking_ms(1);
    _rak3172_reset_line_set(true);
}


static void _rak3172_reset_now(void)
{
    _rak3172_printf("ATZ");
}


static void _rak3172_process_state_off(char* msg)
{
    if (_rak3172_ctx.config_is_valid && msg_is(RAK3172_MSG_INIT, msg))
    {
        comms_debug("READ INIT MESSAGE");
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_BOOT;
        _rak3172_ctx.init_count = 0;
        _rak3172_printf((char*)_rak3172_init_msgs[0]);
    }
}


static void _rak3172_process_state_init_wait_boot(char* msg)
{
    if (msg_is(RAK3172_MSG_INIT, msg))
    {
        comms_debug("READ RE-BOOT MESSAGE");
        _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
        _rak3172_printf((char*)_rak3172_init_msgs[++_rak3172_ctx.init_count]);
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
    /* Second command now reboots the device by the looks, so allow that
     * to pass with init message. */
    if ((_rak3172_ctx.init_count == 1 && msg_is(RAK3172_MSG_INIT, msg)) ||
        msg_is(RAK3172_MSG_OK, msg))
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

        /* ATE is 3rd command so first 3 commands wont have replay */
        if (_rak3172_ctx.init_count > 2)
        {
            _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_REPLAY;
        }
        else
        {
            _rak3172_ctx.state = RAK3172_STATE_INIT_WAIT_OK;
        }
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
        _rak3172_ctx.reset_count = 0;
        _rak3172_ctx.state = RAK3172_STATE_IDLE;
    }
    else if (msg_is(RAK3172_MSG_JOIN_FAILED, msg))
    {
        comms_debug("READ JOIN FAILED");
        rak3172_reset();
    }
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
        _rak3172_ctx.reset_count = 0;
        _rak3172_ctx.state = RAK3172_STATE_IDLE;
        on_protocol_sent_ack(true);

        return;
    }
    if (msg_is(RAK3172_MSG_NACK, msg))
    {
        comms_debug("READ NO SEND ACK");
        rak3172_reset();
        return;
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


static bool _rak3172_load_config(cmd_ctx_t * ctx)
{
    if (!lw_persist_data_is_valid())
    {
        return false;
    }

    lw_config_t* config = lw_get_config();
    if (!config)
        return false;

    /* If outside range, default to LW_REGION_EU868 */
    lw_region_t region;
    if (config->region > LW_REGION_MAX)
    {
        cmd_ctx_error(ctx,"Invalid region, setting to EU868.");
        region = LW_REGION_EU868;
    }
    else
    {
        region = config->region;
    }

    snprintf(
        _rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-4],
        RAK3172_INIT_MSG_LEN,
        "AT+BAND=%"PRIu8,
        (uint8_t)region);

    snprintf(
        _rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-3],
        RAK3172_INIT_MSG_LEN,
        "AT+DEVEUI=%.*s",
        LW_DEV_EUI_LEN,
        config->dev_eui);

    snprintf(
        _rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-2],
        RAK3172_INIT_MSG_LEN,
        "AT+APPEUI=%.*s",
        LW_DEV_EUI_LEN,
        config->dev_eui);

    snprintf(
        _rak3172_init_msgs[ARRAY_SIZE(_rak3172_init_msgs)-1],
        RAK3172_INIT_MSG_LEN,
        "AT+APPKEY=%.*s",
        LW_APP_KEY_LEN,
        config->app_key);

    return true;
}


void rak3172_init(void)
{
    _rak3172_ctx.config_is_valid = _rak3172_load_config(&uart_cmd_ctx);
    if (!_rak3172_ctx.config_is_valid)
    {
        comms_debug("Config is incorrect, not initialising.");
    }
    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.reset_pin.port));
    gpio_mode_setup(_rak3172_ctx.reset_pin.port,
                    GPIO_MODE_INPUT,
                    GPIO_PUPD_NONE,
                    _rak3172_ctx.reset_pin.pins);
    rcc_periph_clock_enable(PORT_TO_RCC(_rak3172_ctx.boot_pin.port));
    gpio_mode_setup(_rak3172_ctx.boot_pin.port,
                    GPIO_MODE_INPUT,
                    GPIO_PUPD_NONE,
                    _rak3172_ctx.boot_pin.pins);
    _rak3172_ctx.state = RAK3172_STATE_OFF;
}


static const char* _rak3172_state_to_str(rak3172_state_t state);


void rak3172_reset(void)
{
    comms_debug("CALLED RESET");
    comms_debug("STATE = %s", _rak3172_state_to_str(_rak3172_ctx.state));
    if (_rak3172_ctx.state == RAK3172_STATE_RESETTING)
        return;
    _rak3172_ctx.state = RAK3172_STATE_RESETTING;

    if (_rak3172_ctx.reset_count < RAK3172_SHORT_RESET_COUNT)
        _rak3172_ctx.reset_count++;

    _rak3172_ctx.sleep_from_time = get_since_boot_ms();
}


static unsigned _rak3172_cmd_to_ascii(char* data, char* ascii)
{
    unsigned len = strnlen(data, 2*CMD_LINELEN);
    if (len % 2)
    {
        comms_debug("Data misaligned to convert to ascii.");
        return 0;
    }
    char* p = ascii;
    for (unsigned i = 0; i < len; i+=2)
    {
        char letter = (char)lw_consume(&data[i], 2);
        if (!isascii(letter))
        {
            comms_debug("Non-ascii character '0x%"PRIx8"'", letter);
            return 0;
        }
        *p = letter;
        p++;
    }
    return len / 2;
}


static void _rak3172_process_unsol2(uint8_t fport, char* data)
{
    char* p = data;
    unsigned len = strlen(p);
    if (lw_consume(p, 2) != LW_UNSOL_VERSION)
        return;
    p += 2;
    uint32_t pl_id = (uint32_t)lw_consume(p, 8);
    p += 8;
    switch (pl_id)
    {
        case LW_ID_CMD:
        {
            comms_debug("Message is command.");
            unsigned ascii_len = _rak3172_cmd_to_ascii(p, _rak3172_ascii_cmd);
            cmds_process(_rak3172_ascii_cmd, ascii_len, NULL);
            break;
        }
        case LW_ID_CCMD:
        {
            comms_debug("Message is confirmed command.");
            unsigned ascii_len = _rak3172_cmd_to_ascii(p, _rak3172_ascii_cmd);
            _rak3172_ctx.err_code = cmds_process(_rak3172_ascii_cmd, ascii_len, NULL);
            comms_debug("Command exited with ERR: %"PRIu8, _rak3172_ctx.err_code);
            break;
        }
        case LW_ID_FW_START:
        {
            comms_debug("Message is fw start.");
            uint16_t count = (uint16_t)lw_consume(p, 4);
            comms_debug("FW of %"PRIu16" chunks", count);
            _rak3172_next_fw_chunk_id = 0;
            fw_ota_reset();
            break;
        }
        case LW_ID_FW_CHUNK:
        {
            uint16_t chunk_id = (uint16_t)lw_consume(p, 4);
            p += 4;
            unsigned chunk_len = len - ((uintptr_t)p - (uintptr_t)data);
            comms_debug("FW chunk %"PRIu16" len %u", chunk_id, chunk_len/2);
            if (_rak3172_next_fw_chunk_id != chunk_id)
            {
                log_error("FW chunk %"PRIu16" ,expecting %"PRIu16, chunk_id, _rak3172_next_fw_chunk_id);
                return;
            }
            _rak3172_next_fw_chunk_id = chunk_id + 1;
            char * p_end = p + chunk_len;
            while(p < p_end)
            {
                uint8_t b = (uint8_t)lw_consume(p, 2);
                p += 2;
                if (!fw_ota_add_chunk(&b, 1, NULL))
                    break;
            }
            break;
        }
        case LW_ID_FW_COMPLETE:
        {
            comms_debug("Message is fw complete.");
            if (len < 12 || !_rak3172_next_fw_chunk_id)
            {
                log_error("RAK4270 FW Finish invalid");
                return;
            }
            uint16_t crc = (uint16_t)lw_consume(p, 4);
            fw_ota_complete(crc);
            _rak3172_next_fw_chunk_id = 0;
            break;
        }
        default:
        {
            comms_debug("Unknown unsol ID 0x%"PRIx32, pl_id);
            break;
        }
    }
}


static void _rak3172_process_unsol(char* msg)
{
    /* +EVT:RX_C:-38:5:UNICAST:1:01434d44006d6561737572656d */
    unsigned len = strlen(msg);
    const char evt[] = "+EVT:RX_C:";
    unsigned evt_len = strlen(evt);
    if (len < evt_len)
    {
        comms_debug("Too short for unsol.");
        return;
    }
    if (strncmp(msg, evt, evt_len) != 0)
    {
        comms_debug("Does not match event.");
        return;
    }
    char * p, * np;
    p = msg + evt_len;
    strtol(p, &np, 10);
    if (p == np)
    {
        comms_debug("No RSSI given.");
        return;
    }
    p = np;
    if (*p != ':')
    {
        comms_debug("Incorrect syntax");
        return;
    }
    p++;
    strtol(p, &np, 10);
    if (p == np)
    {
        comms_debug("No SNR given.");
        return;
    }
    p = np;
    len = p - msg;
    const char unicast[] = ":UNICAST:";
    const unsigned unicast_len = strlen(unicast);
    if (len < unicast_len)
    {
        comms_debug("Too short for UNICAST.");
        return;
    }
    if (strncmp(p, unicast, unicast_len) != 0)
    {
        comms_debug("Does not match UNICAST.");
        return;
    }
    p += unicast_len;
    uint8_t fport = strtoul(p, &np, 10);
    if (p == np)
    {
        comms_debug("No port given.");
        return;
    }
    p = np;
    if (*p != ':')
    {
        comms_debug("Incorrect syntax.");
        return;
    }
    p++;
    unsigned len_left = p - msg;
    for (unsigned i = 0; i < len_left; i++)
    {
        if (!isxdigit(p[i]))
        {
            comms_debug("Data is not ascii.");
            return;
        }
    }
    _rak3172_process_unsol2(fport, p);
}


static char* _rak3172_skip_to_msg(char* msg)
{
    unsigned len = strnlen(msg, RAK3172_MAX_CMD_LEN-1);
    for (unsigned i = 0; i < len; i++)
    {
        if (msg[len] == '+')
        {
            return msg + len;
        }
    }
    return msg;
}


void rak3172_process(char* msg)
{
    char* p = _rak3172_skip_to_msg(msg);
    _rak3172_process_unsol(p);
    switch (_rak3172_ctx.state)
    {
        case RAK3172_STATE_OFF:
            _rak3172_process_state_off(p);
            break;
        case RAK3172_STATE_INIT_WAIT_BOOT:
            _rak3172_process_state_init_wait_boot(p);
            break;
        case RAK3172_STATE_INIT_WAIT_REPLAY:
            _rak3172_process_state_init_wait_replay(p);
            break;
        case RAK3172_STATE_INIT_WAIT_OK:
            _rak3172_process_state_init_wait_ok(p);
            break;
        case RAK3172_STATE_JOIN_WAIT_REPLAY:
            _rak3172_process_state_join_wait_replay(p);
            break;
        case RAK3172_STATE_JOIN_WAIT_OK:
            _rak3172_process_state_join_wait_ok(p);
            break;
        case RAK3172_STATE_JOIN_WAIT_JOIN:
            _rak3172_process_state_join_wait_join(p);
            break;
        case RAK3172_STATE_RESETTING:
            break;
        case RAK3172_STATE_IDLE:
            break;
        case RAK3172_STATE_SEND_WAIT_REPLAY:
            _rak3172_process_state_send_replay(p);
            break;
        case RAK3172_STATE_SEND_WAIT_OK:
            _rak3172_process_state_send_ok(p);
            break;
        case RAK3172_STATE_SEND_WAIT_ACK:
            _rak3172_process_state_send_ack(p);
            break;
        default:
            comms_debug("Unknown state. (%d)", _rak3172_ctx.state);
            return;
    }
}


static bool _rak3172_wait_send(void* userdata)
{
    return !(since_boot_delta(get_since_boot_ms(), _rak3172_ctx.cmd_last_sent) < RAK3172_SEND_DELAY_MS);
}


bool rak3172_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_rak3172_ctx.state != RAK3172_STATE_IDLE)
    {
        comms_debug("Incorrect state to send : %s",
            _rak3172_state_to_str((unsigned)_rak3172_ctx.state));
        return false;
    }

    char send_header[RAK3172_MSG_SEND_HEADER_LEN];

    snprintf(
        send_header,
        RAK3172_MSG_SEND_HEADER_LEN,
        RAK3172_MSG_SEND_HEADER_FMT,
        _rak3172_get_port());

    main_loop_iterate_for(RAK3172_SEND_DELAY_MS, _rak3172_wait_send, NULL);

    if (!_rak3172_write(send_header))
    {
        comms_debug("Could not write SEND header.");
        return false;
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
    return true;
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
        {
            uint32_t now = get_since_boot_ms();
            if (since_boot_delta(now, _rak3172_ctx.sleep_from_time) > RAK3172_LONG_RESET_TIME_MS)
            {
                /* Most likely here from trying to reset, but not
                 * resetting successfully */
                _rak3172_ctx.sleep_from_time = now;
                _rak3172_hard_reset();
            }
            break;
        }
        case RAK3172_STATE_IDLE:
            if (_rak3172_ctx.err_code)
            {
                protocol_send_error_code(_rak3172_ctx.err_code);
                _rak3172_ctx.err_code = 0;
            }
            break;
        case RAK3172_STATE_RESETTING:
        {
            uint32_t sleep_delay = _rak3172_ctx.reset_count >= RAK3172_SHORT_RESET_COUNT ? RAK3172_LONG_RESET_TIME_MS : RAK3172_SHORT_RESET_TIME_MS;
            uint32_t now = get_since_boot_ms();
            if (since_boot_delta(now, _rak3172_ctx.sleep_from_time) > sleep_delay)
            {
                _rak3172_ctx.sleep_from_time = now;
                _rak3172_ctx.state = RAK3172_STATE_OFF;
                _rak3172_reset_now();
            }
            break;
        }
        case RAK3172_STATE_SEND_WAIT_ACK:
            if (since_boot_delta(get_since_boot_ms(), _rak3172_ctx.cmd_last_sent) > RAK3172_ACK_TIMEOUT_MS)
            {
                comms_debug("TIMED OUT WAITING FOR ACK");
                on_protocol_sent_ack(false);
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


void _rak3172_send_alive(void)
{
    if (!rak3172_send_ready())
    {
        comms_debug("Attempted to send alive packet, not in idle state");
        return;
    }

    char send_packet[RAK3172_MSG_SEND_HEADER_LEN];

    unsigned len = snprintf(
        send_packet,
        RAK3172_MSG_SEND_HEADER_LEN-1,
        RAK3172_MSG_SEND_HEADER_FMT,
        _rak3172_get_port());

    send_packet[len] = 0;

    comms_debug("Sending an 'is alive' packet.");
    _rak3172_write(send_packet);
    _rak3172_write("1234\r\n");
    return;
}


command_response_t rak3172_cmd_config_cb(char* str, cmd_ctx_t * ctx)
{
    if (lw_config_setup_str(str, ctx))
    {
        _rak3172_ctx.config_is_valid = _rak3172_load_config(ctx);
        if (_rak3172_ctx.config_is_valid)
        {
            _rak3172_ctx.reset_count = 0;
            rak3172_reset();
        }
        return COMMAND_RESP_OK;
    }
    return COMMAND_RESP_ERR;
}


bool rak3172_get_id(char* str, uint8_t len)
{
    return lw_get_id(str, len);
}


static command_response_t _rak3172_print_boot_reset_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"BOOT = %"PRIu8, (uint8_t)_rak3172_boot_enabled);
    cmd_ctx_out(ctx,"RESET = %"PRIu8, (uint8_t)_rak3172_reset_enabled);
    return COMMAND_RESP_OK;
}


static command_response_t _rak3172_boot_cb(char* args, cmd_ctx_t * ctx)
{
    bool enabled = strtoul(args, NULL, 10);
    if (enabled)
    {
        gpio_mode_setup(_rak3172_ctx.boot_pin.port,
                        GPIO_MODE_OUTPUT,
                        GPIO_PUPD_NONE,
                        _rak3172_ctx.boot_pin.pins);
        gpio_set(_rak3172_ctx.boot_pin.port, _rak3172_ctx.boot_pin.pins);
    }
    else
    {
        gpio_mode_setup(_rak3172_ctx.boot_pin.port,
                        GPIO_MODE_INPUT,
                        GPIO_PUPD_NONE,
                        _rak3172_ctx.boot_pin.pins);
    }
    _rak3172_boot_enabled = enabled;
    return _rak3172_print_boot_reset_cb("", ctx);
}


static command_response_t _rak3172_reset_cb(char* args, cmd_ctx_t * ctx)
{
    bool enabled = strtoul(args, NULL, 10);
    _rak3172_reset_line_set(enabled);
    return _rak3172_print_boot_reset_cb("", ctx);
}


static const char* _rak3172_state_to_str(rak3172_state_t state)
{
    static const char state_strs[12][32] =
    {
        {"RAK3172_STATE_OFF"},
        {"RAK3172_STATE_INIT_WAIT_BOOT"},
        {"RAK3172_STATE_INIT_WAIT_OK"},
        {"RAK3172_STATE_INIT_WAIT_REPLAY"},
        {"RAK3172_STATE_JOIN_WAIT_OK"},
        {"RAK3172_STATE_JOIN_WAIT_REPLAY"},
        {"RAK3172_STATE_JOIN_WAIT_JOIN"},
        {"RAK3172_STATE_RESETTING"},
        {"RAK3172_STATE_IDLE"},
        {"RAK3172_STATE_SEND_WAIT_REPLAY"},
        {"RAK3172_STATE_SEND_WAIT_OK"},
        {"RAK3172_STATE_SEND_WAIT_ACK"},
    };
    static const char none[] = "";
    if (state >= ARRAY_SIZE(state_strs))
        return none;
    return state_strs[state];
}


static const char* _rak3172_init_count_to_str(uint8_t init_count)
{
    static const char none[] = "";
    if (init_count >= ARRAY_SIZE(_rak3172_init_msgs))
        return none;
    return _rak3172_init_msgs[init_count];
}


static command_response_t _rak3172_state_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"STATE: %s (%d)", _rak3172_state_to_str(_rak3172_ctx.state), _rak3172_ctx.state);
    switch (_rak3172_ctx.state)
    {
        case RAK3172_STATE_INIT_WAIT_OK:
            /* Fall through */
        case RAK3172_STATE_INIT_WAIT_REPLAY:
            cmd_ctx_out(ctx,"INIT COUNT         : %"PRIu8,  _rak3172_ctx.init_count);
            cmd_ctx_out(ctx,"INIT MESSAGE       : %s",      _rak3172_init_count_to_str(_rak3172_ctx.init_count));
            break;
        case RAK3172_STATE_RESETTING:
        {
            uint32_t now         = get_since_boot_ms();
            uint32_t sleep_delay = _rak3172_ctx.reset_count >= RAK3172_SHORT_RESET_COUNT ? RAK3172_LONG_RESET_TIME_MS : RAK3172_SHORT_RESET_TIME_MS;
            cmd_ctx_out(ctx,"SLEEP DELAY        : %"PRIu32" ms", sleep_delay);
            cmd_ctx_out(ctx,"SLEEP FROM         : %"PRIu32" ms", _rak3172_ctx.sleep_from_time);
            uint32_t sleep_until = _rak3172_ctx.sleep_from_time + sleep_delay;
            cmd_ctx_out(ctx,"SLEEP UNTIL        : %"PRIu32" ms", sleep_until);
            if (since_boot_delta(now, _rak3172_ctx.sleep_from_time) > sleep_delay)
            {
                cmd_ctx_out(ctx,"TIME UNTIL WAKEUP  : IMMINENT");
                break;
            }
            uint32_t wakeup_time = since_boot_delta(sleep_until, now);
            cmd_ctx_out(ctx,"TIME UNTIL WAKEUP  : %"PRIu32" ms", wakeup_time);
            break;
        }
        default:
            break;
    }
    return COMMAND_RESP_OK;
}


static command_response_t _rak3172_restart_cb(char* args, cmd_ctx_t * ctx)
{
    _rak3172_ctx.state          = RAK3172_STATE_OFF;
    _rak3172_ctx.reset_count    = 0;
    _rak3172_reset_now();
    return COMMAND_RESP_OK;
}


static command_response_t _rak3172_join(char* str, cmd_ctx_t * ctx)
{
    _rak3172_send_alive();
    return COMMAND_RESP_OK;
}


command_response_t rak3172_cmd_conn_cb(char* str, cmd_ctx_t * ctx)
{
    if (rak3172_get_connected())
    {
        cmd_ctx_out(ctx,"1 | Connected");
        return COMMAND_RESP_OK;
    }
    cmd_ctx_out(ctx,"0 | Disconnected");
    return COMMAND_RESP_ERR;
}


static command_response_t _rak3172_tx_power_cb(char* str, cmd_ctx_t * ctx)
{
    char* np;
    unsigned pwr = strtoul(str, &np, 10);
    command_response_t status = COMMAND_RESP_OK;
    if (str != np)
    {
        status = _rak3172_printf("AT+TXP=%u", pwr) ? COMMAND_RESP_OK  :
                                                     COMMAND_RESP_ERR ;
    }
    else
    {
        cmd_ctx_out(ctx,"Enter a valid number.");
        status = COMMAND_RESP_ERR;
    }
    return status;
}


static command_response_t _rak3172_trssi_cb(char* str, cmd_ctx_t * ctx)
{
    return _rak3172_printf("AT+TRSSI=?") ? COMMAND_RESP_OK  :
                                           COMMAND_RESP_ERR ;
}


static command_response_t _rak3172_ttx_cb(char* str, cmd_ctx_t * ctx)
{
    char* np;
    unsigned inp = strtoul(str, &np, 10);
    command_response_t status = COMMAND_RESP_OK;
    if (str != np)
    {
        status = _rak3172_printf("AT+TTX=%u", inp) ? COMMAND_RESP_OK  :
                                                     COMMAND_RESP_ERR ;
    }
    else
    {
        cmd_ctx_out(ctx,"Enter a valid number.");
        status = COMMAND_RESP_ERR;
    }
    return status;
}


static command_response_t _rak3172_trx_cb(char* str, cmd_ctx_t * ctx)
{
    char* np;
    unsigned inp = strtoul(str, &np, 10);
    command_response_t status = COMMAND_RESP_OK;
    if (str != np)
    {
        status = _rak3172_printf("AT+TRX=%u", inp) ? COMMAND_RESP_OK  :
                                                     COMMAND_RESP_ERR ;
    }
    else
    {
        cmd_ctx_out(ctx,"Enter a valid number.");
        status = COMMAND_RESP_ERR;
    }
    return status;
}


command_response_t rak3172_cmd_j_cfg_cb(char* str, cmd_ctx_t * ctx)
{
    lw_print_config(ctx);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* rak3172_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_print",  "Print boot/reset line",       _rak3172_print_boot_reset_cb  , false , NULL },
        { "comms_boot",   "Enable/disable boot line",    _rak3172_boot_cb              , false , NULL },
        { "comms_reset",  "Enable/disable reset line",   _rak3172_reset_cb             , false , NULL },
        { "comms_state",  "Print comms state",           _rak3172_state_cb             , false , NULL },
        { "comms_restart","Comms restart",               _rak3172_restart_cb           , false , NULL },
        { "connect",      "Send an alive packet",        _rak3172_join                 , false , NULL },
        { "comms_txpower", "TX Power",                   _rak3172_tx_power_cb          , false , NULL },
        { "comms_trssi",  "Start RF RSSI tone test",     _rak3172_trssi_cb             , false , NULL },
        { "comms_ttx",    "Start RF TX test",            _rak3172_ttx_cb               , false , NULL },
        { "comms_trx",    "Start RF RX test",            _rak3172_trx_cb               , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}

void rak3172_power_down(void)
{
    _rak3172_ctx.state = RAK3172_STATE_OFF;
}


/* Return false if different
 *        true  if same      */
bool rak3172_persist_config_cmp(void* d0, void* d1)
{
    return lw_persist_config_cmp(
        (lw_config_t*)((comms_config_t*)d0),
        (lw_config_t*)((comms_config_t*)d1));
}
