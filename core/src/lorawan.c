#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "uart_rings.h"
#include "uarts.h"
#include "config.h"
#include "comms.h"
#include "log.h"
#include "cmd.h"
#include "measurements.h"
#include "persist_config.h"
#include "common.h"
#include "timers.h"
#include "update.h"
#include "pinmap.h"

#define LW_HEADER_SIZE                      17
#define LW_TAIL_SIZE                        2

#define LW_PAYLOAD_MAX_DEFAULT              242


_Static_assert (MEASUREMENTS_HEX_ARRAY_SIZE * 2 < LW_PAYLOAD_MAX_DEFAULT, "Measurement send data max longer than LoRaWAN payload max.");

/* AT commands https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4270-Module/AT-Command-Manual */

static bool                 _persist_data_lw_valid = false;

#define LW_BUFFER_SIZE                  UART_1_OUT_BUF_SIZE
#define LW_CONFIG_TIMEOUT_S             30
#define comms_reset_GPIO_DEFAULT_MS        10
#define LW_SLOW_RESET_TIMEOUT_MINS      15
#define LW_MAX_RESEND                   5
#define LW_DELAY_MS                     2

#define LW_ERROR_PREFIX                 "ERROR: "

#define LW_SETTING_JOIN_MODE_OTAA       0
#define LW_SETTING_JOIN_MODE_ABP        1
#define LW_SETTING_CLASS_A              0
#define LW_SETTING_CLASS_C              2
#define LW_SETTING_UNCONFIRM            0
#define LW_SETTING_CONFIRM              1
#define LW_SETTING_ADR_DISABLED         0
#define LW_SETTING_ADR_ENABLED          1
#define LW_SETTING_DR_0                 0
#define LW_SETTING_DR_1                 1
#define LW_SETTING_DR_2                 2
#define LW_SETTING_DR_3                 3
#define LW_SETTING_DR_4                 4
#define LW_SETTING_DR_5                 5
#define LW_SETTING_DR_6                 6
#define LW_SETTING_DR_7                 7
#define LW_SETTING_DUTY_CYCLE_DISABLED  0
#define LW_SETTING_DUTY_CYCLE_ENABLED   1

#define LW_CONFIG_JOIN_MODE             STR(LW_SETTING_JOIN_MODE_OTAA)
#define LW_CONFIG_CLASS                 STR(LW_SETTING_CLASS_C)
#define LW_CONFIG_REGION                "EU868"
#define LW_CONFIG_CONFIRM_TYPE          STR(LW_SETTING_CONFIRM)
#define LW_CONFIG_ADR                   STR(LW_SETTING_ADR_DISABLED)
#define LW_CONFIG_DR                    STR(LW_SETTING_DR_4)
#define LW_CONFIG_DUTY_CYCLE            STR(LW_SETTING_DUTY_CYCLE_DISABLED)

#define LW_UNSOL_VERSION                0x01

#define LW_ID_CMD                       0x434d4400 /* CMD */
#define LW_ID_FW_START                  0x46572d00 /* FW- */
#define LW_ID_FW_CHUNK                  0x46572b00 /* FW+ */
#define LW_ID_FW_COMPLETE               0x46574000 /* FW@ */

typedef enum
{
    LW_STATE_OFF,
    LW_STATE_WAIT_INIT,
    LW_STATE_WAIT_INIT_OK,
    LW_STATE_WAIT_REINIT,
    LW_STATE_WAIT_CONN,
    LW_STATE_WAIT_OK,
    LW_STATE_WAIT_ACK,
    LW_STATE_IDLE,
    LW_STATE_WAIT_UCONF_OK,
    LW_STATE_WAIT_CODE_OK,
    LW_STATE_WAIT_CONF_OK,
    LW_STATE_UNCONFIGURED,
} lw_states_t;


typedef enum
{
    LW_RECV_ACK,
    LW_RECV_DATA,
    LW_RECV_ERR_NOT_START,
    LW_RECV_ERR_BAD_FMT,
    LW_RECV_ERR_UNFIN,
} lw_recv_packet_types_t;


typedef enum
{
    LW_ERROR_CMD_UNSUPP         =  1,   // reset
    LW_ERROR_INVALID_CMD        =  2,   // reset
    LW_ERROR_R_W_FLASH          =  3,   // reset
    LW_ERROR_UART_ERROR         =  5,   // reset
    LW_ERROR_TRANS_BUSY         =  80,  //  retry
    LW_ERROR_SVC_UNKNOWN        =  81,  // reset
    LW_ERROR_INVLD_LORA_PARA    =  82,  // reset
    LW_ERROR_INVLD_LORA_FREQ    =  83,  // reset
    LW_ERROR_INVLD_LORA_DR      =  84,  // reset
    LW_ERROR_INVLD_LORA_FR_DR   =  85,  // reset
    LW_ERROR_NOT_CONNECTED      =  86,  // reset
    LW_ERROR_PACKET_SIZE        =  87,  //  reduce limit, reset, drop data
    LW_ERROR_SVC_CLOSED         =  88,  // reset
    LW_ERROR_REGION_UNSUPP      =  89,  // reset
    LW_ERROR_DUTY_CYCLE_CLSD    =  90,  // reset
    LW_ERROR_CHAN_NO_VLD_FND    =  91,  // reset
    LW_ERROR_CHAN_NO_AVLBL_FND  =  92,  // reset
    LW_ERROR_STATUS_ERROR       =  93,  // reset
    LW_ERROR_TIMEOUT_LORA_TRANS =  94,  // reset
    LW_ERROR_TIMEOUT_RX1        =  95,  //  nothing
    LW_ERROR_TIMEOUT_RX2        =  96,  //  retry
    LW_ERROR_RECV_RX1           =  97,  //  retry
    LW_ERROR_RECV_RX2           =  98,  //  retry
    LW_ERROR_JOIN_FAIL          =  99,  // reset
    LW_ERROR_DUP_DOWNLINK       = 100,  //  nothing
    LW_ERROR_PAYLOAD_SIZE       = 101,  //  reduce limit, reset, drop data
    LW_ERROR_MANY_PKTS_LOST     = 102,  // reset
    LW_ERROR_ADDR_FAIL          = 103,  // reset
    LW_ERROR_INVLD_MIC          = 104,  //  nothing
} lw_error_codes_t;


typedef enum
{
    LW_CMD_ERROR_NO_ERROR       = 0,
} lw_cmd_error_codes_t;


typedef enum
{
    LW_BKUP_MSG_BLANK,
    LW_BKUP_MSG_STR,
    LW_BKUP_MSG_HEX,
} lw_backup_msg_types_t;


typedef struct
{
    union
    {
        struct
        {
            int16_t port;
            int16_t rssi;
            int16_t snr;
            int16_t datalen;
        };
        int16_t raw[4];
    } header;
    char* data;
} lw_payload_t;


typedef struct
{
    lw_states_t state;
    unsigned    init_step;
    uint32_t    last_message_time;
    uint8_t     resend_count;
    uint8_t     reset_count;
} lw_state_machine_t;


typedef char lw_msg_buf_t[64];


typedef struct
{
    lw_backup_msg_types_t backup_type;
    union
    {
        char    string[LW_BUFFER_SIZE];
        struct
        {
            uint8_t len;
            int8_t arr[MEASUREMENTS_HEX_ARRAY_SIZE];
        } hex;
    };
} lw_backup_msg_t;


typedef struct
{
    lw_cmd_error_codes_t    code;
    bool                    valid;
} error_code_t;


static lw_state_machine_t   _lw_state_machine                   = {.state=LW_STATE_OFF, .init_step=0, .last_message_time=0, .resend_count=0, .reset_count=0};
static port_n_pins_t        _lw_reset_gpio                      = { GPIOC, GPIO8 };
static char                 _lw_out_buffer[LW_BUFFER_SIZE]      = {0};
static uint8_t              _lw_port                            = 0;
static uint32_t             _lw_chip_off_time                   = 0;
static uint32_t             _lw_reset_timeout                   = comms_reset_GPIO_DEFAULT_MS;
static lw_backup_msg_t      _lw_backup_message                  = {.backup_type=LW_BKUP_MSG_BLANK, .hex={.len=0, .arr={0}}};
static error_code_t         _lw_error_code                      = {0, false};
static char                 _lw_cmd_ascii[CMD_LINELEN]          = "";
static uint16_t             _next_fw_chunk_id                   = 0;


static uint16_t             _lw_packet_max_size                  = LW_PAYLOAD_MAX_DEFAULT;


static lw_msg_buf_t _init_msgs[] = { "at+set_config=lora:default_parameters",
                                     "at+set_config=lora:join_mode:"LW_CONFIG_JOIN_MODE,
                                     "at+set_config=lora:class:"LW_CONFIG_CLASS,
                                     "at+set_config=lora:region:"LW_CONFIG_REGION,
                                     "at+set_config=lora:confirm:"LW_CONFIG_CONFIRM_TYPE,
                                     "at+set_config=lora:adr:"LW_CONFIG_ADR,
                                     "at+set_config=lora:dr:"LW_CONFIG_DR,
                                     "at+set_config=lora:dutycycle_enable:"LW_CONFIG_DUTY_CYCLE,
                                     "Dev EUI goes here",
                                     "App EUI goes here",
                                     "App Key goes here"};


uint16_t comms_get_mtu(void)
{
    return _lw_packet_max_size;
}


static void _lw_chip_off(void)
{
    _lw_state_machine.state = LW_STATE_OFF;
    gpio_clear(_lw_reset_gpio.port, _lw_reset_gpio.pins);
}


static void _lw_msg_delay(void)
{
    spin_blocking_ms(LW_DELAY_MS);
}


static void _lw_chip_on(void)
{
    _lw_state_machine.state = LW_STATE_WAIT_INIT;
    _lw_state_machine.init_step = 0;
    gpio_set(_lw_reset_gpio.port, _lw_reset_gpio.pins);
}


static void _lw_update_last_message_time(void)
{
    _lw_state_machine.last_message_time = get_since_boot_ms();
}


static unsigned _lw_write_to_uart(char* string)
{
    _lw_update_last_message_time();
    return uart_ring_out(LW_UART, string, strlen(string));
}


static void _lw_write(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_lw_out_buffer, LW_BUFFER_SIZE, fmt, args);
    va_end(args);
    comms_debug("<< %s", _lw_out_buffer);
    size_t len = strlen(_lw_out_buffer);
    _lw_out_buffer[len] = '\r';
    _lw_out_buffer[len+1] = '\n';
    _lw_out_buffer[len+2] = 0;
    _lw_write_to_uart(_lw_out_buffer);
}


static uint8_t _lw_get_port(void)
{
    _lw_port++;
    if (_lw_port > 223)
    {
        _lw_port = 0;
    }
    return _lw_port;
}


static void _lw_reset_gpio_init(void)
{
    rcc_periph_clock_enable(PORT_TO_RCC(_lw_reset_gpio.port));
    gpio_mode_setup(_lw_reset_gpio.port,
                    GPIO_MODE_OUTPUT,
                    GPIO_PUPD_NONE,
                    _lw_reset_gpio.pins);
}


static lw_config_t* _lw_get_config(void)
{
    comms_config_t* comms_config = persist_get_comms_config();
    if (comms_config->type != COMMS_TYPE_LW)
    {
        comms_debug("Tried to get config for LORAWAN but config is not for LORAWAN.");
        return NULL;
    }
    return (lw_config_t*)(comms_config->setup);
}


static bool _lw_load_config(void)
{
    if (_persist_data_lw_valid)
    {
        log_error("No LoRaWAN Dev EUI and/or App Key.");
        return false;
    }

    lw_config_t* config = _lw_get_config();
    if (!config)
        return false;

    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-3], sizeof(lw_msg_buf_t), "at+set_config=lora:dev_eui:%."STR(LW_DEV_EUI_LEN)"s", config->dev_eui);
    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-2], sizeof(lw_msg_buf_t), "at+set_config=lora:app_eui:%."STR(LW_APP_KEY_LEN)"s", config->dev_eui);
    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-1], sizeof(lw_msg_buf_t), "at+set_config=lora:app_key:%."STR(LW_APP_KEY_LEN)"s", config->app_key);
    return true;
}


static void _lw_chip_init(void)
{
    if (_lw_state_machine.state != LW_STATE_OFF)
    {
        comms_debug("Chip already on, restarting chip.");
        _lw_chip_off();
        timer_delay_us_64(comms_reset_GPIO_DEFAULT_MS * 1000);
    }
    comms_debug("Initialising LoRaWAN chip.");
    _lw_chip_on();
}


static void _lw_set_confirmed(bool confirmed)
{
    if (confirmed)
    {
        _lw_state_machine.state = LW_STATE_WAIT_CONF_OK;
        comms_debug("Setting to confirmed.");
        _lw_write("at+set_config=lora:confirm:1");
    }
    else
    {
        _lw_state_machine.state = LW_STATE_WAIT_UCONF_OK;
        comms_debug("Setting to unconfirmed.");
        _lw_write("at+set_config=lora:confirm:0");
    }
}


static void _lw_join_network(void)
{
    _lw_write("at+join");
}


static void _lw_soft_reset(void)
{
    _lw_write("at+set_config=device:restart");
}


bool comms_send_ready(void)
{
    return _lw_state_machine.state == LW_STATE_IDLE;
}


bool comms_send_str(char* str)
{
    if (!comms_send_ready())
    {
        comms_debug("Cannot send '%s' as chip is not in IDLE state.", str);
        return false;
    }
    _lw_state_machine.state = LW_STATE_WAIT_OK;
    _lw_write("at+send=lora:%u:%s", _lw_get_port(), str);
    _lw_backup_message.backup_type = LW_BKUP_MSG_STR;
    strncpy(_lw_backup_message.string, str, strlen(str));
    return true;
}


static void _lw_clear_backup(void)
{
    _lw_backup_message.backup_type = LW_BKUP_MSG_BLANK;
}


static void _lw_retry_write(void)
{
    if (_lw_state_machine.resend_count >= LW_MAX_RESEND)
    {
        comms_debug("Failed to successfully resend (%u times), resetting chip.", LW_MAX_RESEND);
        _lw_state_machine.resend_count = 0;
        comms_reset();
        on_comms_sent_ack(false);
        return;
    }
    _lw_state_machine.resend_count++;
    comms_debug("Resending %"PRIu8"/%u", _lw_state_machine.resend_count, LW_MAX_RESEND);
    switch (_lw_backup_message.backup_type)
    {
        case LW_BKUP_MSG_STR:
            _lw_state_machine.state = LW_STATE_IDLE;
            comms_send_str(_lw_backup_message.string);
            break;
        case LW_BKUP_MSG_HEX:
            _lw_state_machine.state = LW_STATE_IDLE;
            comms_send(_lw_backup_message.hex.arr, _lw_backup_message.hex.len);
            break;
        default:
            comms_debug("Broken backup, cant resend");
            return;
    }
}


 void _lw_send_alive(void)
{
    comms_debug("Sending an 'is alive' packet.");
    _lw_write("at+send=lora:%"PRIu8":", _lw_get_port());
    return;
}


void comms_init(void)
{
    lw_config_t* config = _lw_get_config();
    if (!config)
        return;

    _persist_data_lw_valid = true;

    if (config->dev_eui[0] && config->app_key[0])
    {
        for(unsigned n = 0; n < LW_DEV_EUI_LEN; n++)
        {
            if (!isascii(config->dev_eui[n]))
            {
                _persist_data_lw_valid = false;
                log_error("Persistent data lw dev not valid");
                break;
            }
        }
        for(unsigned n = 0; n < LW_APP_KEY_LEN; n++)
        {
            if (!isascii(config->app_key[n]))
            {
                _persist_data_lw_valid = false;
                log_error("Persistent data lw app not valid");
                break;
            }
        }
    }

    _lw_reset_gpio_init();
    if (_lw_state_machine.state != LW_STATE_OFF)
    {
        log_error("LoRaWAN chip not in DISCONNECTED state.");
        return;
    }
    if (!_lw_load_config())
    {
        log_error("Loading LoRaWAN config failed. Not connecting to network.");
        _lw_state_machine.state = LW_STATE_UNCONFIGURED;
        return;
    }
    _lw_chip_init();
}

static uint64_t _lw_handle_unsol_consume(char *p, unsigned len)
{
    if (len > 16 || (len % 1))
        return 0;

    char tmp = p[len];
    p[len] = 0;
    uint64_t r = strtoull(p, NULL, 16);
    p[len] = tmp;
    return r;
}


static unsigned _lw_handle_unsol_2_lw_cmd_ascii(char *p)
{
    char* lw_p = _lw_cmd_ascii;
    char* lw_p_last = lw_p + CMD_LINELEN - 1;

    while(*p && lw_p < lw_p_last)
    {
        uint8_t val = _lw_handle_unsol_consume(p, 2);
        p+=2;
        if (val != 0)
            *lw_p++ = (char)val;
        else
            break;
    }
    *lw_p = 0;
    return (uintptr_t)lw_p - (uintptr_t)_lw_cmd_ascii;
}


bool comms_send_allowed(void)
{
    // TODO: This function could probably be done better.
    return (_next_fw_chunk_id != 0);
}


static void _lw_handle_unsol(lw_payload_t * incoming_pl)
{
    char* p = incoming_pl->data;

    unsigned len = strlen(p);

    if (len % 1 || len < 10)
    {
        log_error("Invalid LW unsol msg");
        return;
    }

    if (_lw_handle_unsol_consume(p, 2) != LW_UNSOL_VERSION)
    {
        log_error("Couldn't parse LW unsol msg");
        return;
    }
    p += 2;
    uint32_t pl_id = (uint32_t)_lw_handle_unsol_consume(p, 8);
    p += 8;

    switch (pl_id)
    {
        case LW_ID_CMD:
        {
            unsigned cmd_len = _lw_handle_unsol_2_lw_cmd_ascii(p);
            cmds_process(_lw_cmd_ascii, cmd_len);
            /* FIXME: Give cmds an exit code.
            _lw_error_code.code = cmds_process(_lw_cmd_ascii, strlen(_lw_cmd_ascii));
            _lw_error_code.valid = true;
            */
            break;
        }
        case LW_ID_FW_START:
        {
            uint16_t count = (uint16_t)_lw_handle_unsol_consume(p, 4);
            comms_debug("FW of %"PRIu16" chunks", count);
            _next_fw_chunk_id = 0;
            fw_ota_reset();
            break;
        }
        case LW_ID_FW_CHUNK:
        {
            uint16_t chunk_id = (uint16_t)_lw_handle_unsol_consume(p, 4);
            p += 4;
            unsigned chunk_len = len - ((uintptr_t)p - (uintptr_t)incoming_pl->data);
            comms_debug("FW chunk %"PRIu16" len %u", chunk_id, chunk_len/2);
            if (_next_fw_chunk_id != chunk_id)
            {
                log_error("FW chunk %"PRIu16" ,expecting %"PRIu16, chunk_id, _next_fw_chunk_id);
                return;
            }
            _next_fw_chunk_id = chunk_id + 1;
            char * p_end = p + chunk_len;
            while(p < p_end)
            {
                uint8_t b = (uint8_t)_lw_handle_unsol_consume(p, 2);
                p += 2;
                if (!fw_ota_add_chunk(&b, 1))
                    break;
            }
            break;
        }
        case LW_ID_FW_COMPLETE:
        {
            if (len < 12 || !_next_fw_chunk_id)
            {
                log_error("LW FW Finish invalid");
                return;
            }
            uint16_t crc = (uint16_t)_lw_handle_unsol_consume(p, 4);
            fw_ota_complete(crc);
            _next_fw_chunk_id = 0;
            break;
        }
        default:
        {
            comms_debug("Unknown unsol ID 0x%"PRIx32, pl_id);
            break;
        }
    }
}


static lw_recv_packet_types_t _lw_parse_recv(char* message, lw_payload_t* payload)
{
    // at+recv=PORT,RSSI,SNR,DATALEN:DATA
    const char recv_msg[] = "at+recv=";
    char* pos = NULL;
    char* next_pos = NULL;

    if (strncmp(message, recv_msg, strlen(recv_msg)) != 0)
    {
        return LW_RECV_ERR_NOT_START;
    }

    pos = message + strlen(recv_msg);
    for (int i = 0; i < 3; i++)
    {
        payload->header.raw[i] = strtol(pos, &next_pos, 10);
        if ((*next_pos) != ',')
        {
            return LW_RECV_ERR_BAD_FMT;
        }
        pos = next_pos + 1;
    }
    payload->header.datalen = strtol(pos, &next_pos, 10);
    if (*next_pos == '\0')
    {
        payload->data = NULL;
        if (payload->header.datalen == 0)
        {
            return LW_RECV_ACK;
        }
        return LW_RECV_ERR_BAD_FMT;
    }
    if (*next_pos == ':')
    {
        payload->data = next_pos + 1;
        int size_diff = strlen(payload->data)/2 - (int)payload->header.datalen;
        if (size_diff != 0)
        {
            return LW_RECV_ERR_BAD_FMT;
        }
        return LW_RECV_DATA;
    }
    return LW_RECV_ERR_BAD_FMT;
}


static bool _lw_msg_is(const char* ref, char* message)
{
    return (bool)(strncmp(message, ref, strlen(ref)) == 0);
}


static bool _lw_msg_is_error(char* message)
{
    return _lw_msg_is("ERROR:", message);
}


static bool _lw_msg_is_initialisation(char* message)
{
    return _lw_msg_is("Initialization OK", message);
}


static bool _lw_msg_is_ok(char* message)
{
    return _lw_msg_is("OK", message);
}


static bool _lw_msg_is_connected(char* message)
{
    return _lw_msg_is("OK Join Success", message);
}


static bool _lw_msg_is_ack(char* message)
{
    lw_payload_t payload;
    return (_lw_parse_recv(message, &payload) == LW_RECV_ACK);
}


static bool _lw_write_next_init_step(void)
{
    if (_lw_state_machine.init_step >= ARRAY_SIZE(_init_msgs))
    {
        return false;
    }
    _lw_write(_init_msgs[_lw_state_machine.init_step++]);
    return true;
}


static void _lw_send_error_code(void)
{
    // FIXME: No real protocol or anything is set up with this yet.
    if (_lw_error_code.valid)
    {
        char err_msg[11] = "";
        snprintf(err_msg, 11, "%"PRIx16, _lw_error_code.code);
        comms_debug("Sending error message '%s'", err_msg);
        comms_send_str(err_msg);
        _lw_error_code.valid = false;
    }
}


void comms_reset(void)
{
    if (_lw_state_machine.state == LW_STATE_OFF)
    {
        comms_debug("Already resetting.");
        return;
    }
    _lw_chip_off();
    _lw_chip_off_time = get_since_boot_ms();
    if (++_lw_state_machine.reset_count > 2)
    {
        comms_debug("Going into long reset mode (wait %"PRIi16" mins).", LW_SLOW_RESET_TIMEOUT_MINS);
        _lw_reset_timeout = LW_SLOW_RESET_TIMEOUT_MINS * 60 * 1000;
    }
    else
    {
        _lw_reset_timeout = 10;
    }
}


static bool _lw_reload_config(void)
{
    if (!_lw_load_config())
    {
        return false;
    }
    _lw_state_machine.reset_count = 0;
    comms_reset();
    return true;
}


static void _lw_handle_error(char* message)
{
    char* pos = NULL;
    char* next_pos = NULL;
    lw_error_codes_t err_no = 0;

    pos = message + strlen(LW_ERROR_PREFIX);
    err_no = (lw_error_codes_t)strtol(pos, &next_pos, 10);
    switch (err_no)
    {
        case LW_ERROR_TRANS_BUSY:
            comms_debug("LoRa Transceiver is busy, retrying.");
            _lw_retry_write();
            break;
        case LW_ERROR_PACKET_SIZE:
            comms_debug("Packet size too large, reducing limit throwing data and resetting chip.");
            _lw_packet_max_size -= 2;
            on_comms_sent_ack(false);
            comms_reset();
            break;
        case LW_ERROR_TIMEOUT_RX1:
            if (comms_get_connected())
            {
                comms_debug("Timed out waiting for packet in windox RX1.");
                _lw_retry_write();
            }
            else
            {
                comms_debug("Send alive wasn't responded to.");
                comms_reset();
            }
            break;
        case LW_ERROR_TIMEOUT_RX2:
            if (comms_get_connected())
            {
                comms_debug("Timed out waiting for packet in windox RX2.");
                _lw_retry_write();
            }
            else
            {
                comms_debug("Send alive wasn't responded to.");
                comms_reset();
            }
            break;
        case LW_ERROR_RECV_RX1:
            comms_debug("Error receiving message in RX1.");
            _lw_retry_write();
            break;
        case LW_ERROR_RECV_RX2:
            comms_debug("Error receiving message in RX2.");
            _lw_retry_write();
            break;
        case LW_ERROR_DUP_DOWNLINK:
            comms_debug("Duplicate downlink.");
            break;
        case LW_ERROR_PAYLOAD_SIZE:
            comms_debug("Packet size not valid for current data rate, reducing limit throwing data and resetting chip.");
            _lw_packet_max_size -= 2;
            on_comms_sent_ack(false);
            comms_reset();
            break;
        case LW_ERROR_INVLD_MIC:
            comms_debug("Invalid MIC in LoRa message.");
            break;
        default:
            comms_debug("Error Code %"PRIu8", resetting chip.", err_no);
            comms_reset();
            break;
    }
}


static void _lw_process_wait_init(char* message)
{
    if (_lw_msg_is_initialisation(message))
    {
        _lw_state_machine.state = LW_STATE_WAIT_INIT_OK;
        _lw_write_next_init_step();
    }
}


static void _lw_process_wait_init_ok(char* message)
{
    if (_lw_msg_is_ok(message))
    {
        _lw_msg_delay();
        if (_lw_state_machine.init_step == ARRAY_SIZE(_init_msgs))
        {
            _lw_state_machine.state = LW_STATE_WAIT_REINIT;
            _lw_soft_reset();
        }
        else
        {
            _lw_write_next_init_step();
        }
    }
}


static void _lw_process_wait_reinit(char* message)
{
    if (_lw_msg_is_initialisation(message))
    {
        _lw_msg_delay();
        _lw_state_machine.state = LW_STATE_WAIT_CONN;
        _lw_join_network();
    }
}


static void _lw_process_wait_conn(char* message)
{
    if (_lw_msg_is_connected(message))
    {
        _lw_msg_delay();
        _lw_state_machine.state = LW_STATE_WAIT_OK;
        _lw_send_alive();
    }
}


static void _lw_process_wait_ok(char* message)
{
    if (_lw_msg_is_ok(message))
    {
        _lw_state_machine.state = LW_STATE_WAIT_ACK;
    }
}


static void _lw_process_wait_ack(char* message)
{
    if (_lw_msg_is_ack(message))
    {
        _lw_state_machine.state = LW_STATE_IDLE;
        _lw_state_machine.reset_count = 0;
        _lw_state_machine.resend_count = 0;
        _lw_clear_backup();
        on_comms_sent_ack(true);
    }
}


static void _lw_process_wait_uconf_ok(char* message)
{
    if (_lw_msg_is_ok(message))
    {
        _lw_state_machine.state = LW_STATE_WAIT_CODE_OK;
        _lw_send_error_code();
    }
}


static void _lw_process_wait_code_ok(char* message)
{
    if (_lw_msg_is_ok(message))
    {
        _lw_state_machine.state = LW_STATE_WAIT_CONF_OK;
        _lw_set_confirmed(true);
    }
}


static void _lw_process_wait_conf_ok(char* message)
{
    if (_lw_msg_is_ok(message))
    {
        _lw_state_machine.state = LW_STATE_IDLE;
    }
}


void comms_process(char* message)
{
    _lw_update_last_message_time();
    if (_lw_msg_is_error(message))
    {
        _lw_handle_error(message);
        return;
    }

    lw_payload_t payload;
    lw_recv_packet_types_t recv_type = _lw_parse_recv(message, &payload);

    if (recv_type == LW_RECV_ERR_BAD_FMT)
    {
        log_error("Invalid recv");
        return;
    }

    if (recv_type == LW_RECV_DATA)
    {
        _lw_handle_unsol(&payload);
        return;
    }

    if (recv_type == LW_RECV_ACK &&
        _lw_state_machine.state != LW_STATE_WAIT_ACK)
    {
        comms_debug("Unexpected ACK");
        return;
    }

    switch(_lw_state_machine.state)
    {
        case LW_STATE_WAIT_INIT:
            _lw_process_wait_init(message);
            break;
        case LW_STATE_WAIT_INIT_OK:
            _lw_process_wait_init_ok(message);
            break;
        case LW_STATE_WAIT_REINIT:
            _lw_process_wait_reinit(message);
            break;
        case LW_STATE_WAIT_CONN:
            _lw_process_wait_conn(message);
            break;
        case LW_STATE_WAIT_OK:
            _lw_process_wait_ok(message);
            break;
        case LW_STATE_WAIT_ACK:
            _lw_process_wait_ack(message);
            break;
        case LW_STATE_IDLE:
            break;
        case LW_STATE_WAIT_UCONF_OK:
            _lw_process_wait_uconf_ok(message);
            break;
        case LW_STATE_WAIT_CODE_OK:
            _lw_process_wait_code_ok(message);
            break;
        case LW_STATE_WAIT_CONF_OK:
            _lw_process_wait_conf_ok(message);
            break;
        case LW_STATE_UNCONFIGURED:
            /* Also panic? */
            break;
        case LW_STATE_OFF:
            /* More panic */
            break;
    }
}


static unsigned _lw_send_size(uint16_t arr_len)
{
    unsigned next_lw_port = _lw_port;
    if (next_lw_port > 223)
        next_lw_port = 0;

    unsigned size = (LW_HEADER_SIZE + (arr_len * 2) + LW_TAIL_SIZE);

    if (next_lw_port < 99)
    {
        size--;
        if (next_lw_port < 9)
            size--;
    }

    return size;
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_lw_state_machine.state == LW_STATE_IDLE)
    {
        char header_str[LW_HEADER_SIZE + 1] = {0};
        char hex_str[3] = {0};
        unsigned expected = _lw_send_size(arr_len);
        unsigned header_size = snprintf(header_str, sizeof(header_str), "at+send=lora:%"PRIu8":", _lw_get_port());
        unsigned sent = 0;
        _lw_msg_delay();
        sent += _lw_write_to_uart(header_str);
        const char desc[]  ="LORA >> ";
        uart_ring_out(CMD_UART, desc, strlen(desc));
        uart_ring_out(CMD_UART, header_str, header_size);
        for (uint16_t i = 0; i < arr_len; i++)
        {
            snprintf(hex_str, 3, "%02"PRIx8, hex_arr[i]);
            sent += _lw_write_to_uart(hex_str);
            uart_ring_out(CMD_UART, hex_str, 2);
        }
        sent += _lw_write_to_uart("\r\n");
        uart_ring_out(CMD_UART, "\r\n", 2);
        _lw_state_machine.state = LW_STATE_WAIT_OK;

        if (_lw_backup_message.hex.arr != hex_arr)
        {
            _lw_backup_message.backup_type = LW_BKUP_MSG_HEX;
            _lw_backup_message.hex.len = arr_len;
            memcpy(_lw_backup_message.hex.arr, hex_arr, arr_len);
        }

        if (sent != expected)
            log_error("Failed to send all bytes over LoRaWAN (%u != %u)", sent, expected);
        else
            comms_debug("Sent %u bytes", sent);
    }
    else
    {
        comms_debug("Incorrect state to send : %u", (unsigned)_lw_state_machine.state);
    }
}


bool comms_get_connected(void)
{
    return (_lw_state_machine.state == LW_STATE_WAIT_OK       ||
            _lw_state_machine.state == LW_STATE_WAIT_ACK      ||
            _lw_state_machine.state == LW_STATE_IDLE          ||
            _lw_state_machine.state == LW_STATE_WAIT_UCONF_OK ||
            _lw_state_machine.state == LW_STATE_WAIT_CODE_OK  ||
            _lw_state_machine.state == LW_STATE_WAIT_CONF_OK  );
}


void comms_loop_iteration(void)
{
    uint32_t now = get_since_boot_ms();
    switch(_lw_state_machine.state)
    {
        case LW_STATE_OFF:
            if (since_boot_delta(now, _lw_chip_off_time) > _lw_reset_timeout)
            {
                _lw_chip_on();
            }
            break;
        case LW_STATE_IDLE:
            break;
        case LW_STATE_UNCONFIGURED:
            break;
        default:
            if (since_boot_delta(get_since_boot_ms(), _lw_state_machine.last_message_time) > LW_CONFIG_TIMEOUT_S * 1000)
            {
                if (_lw_state_machine.state == LW_STATE_WAIT_OK || _lw_state_machine.state == LW_STATE_WAIT_ACK)
                {
                    on_comms_sent_ack(false);
                }
                comms_debug("LoRa chip timed out, resetting.");
                comms_reset();
            }
            break;
    }
}


void     comms_config_setup_str(char * str)
{
    // CMD  : "lora_config dev-eui 118f875d6994bbfd"
    // ARGS : "dev-eui 118f875d6994bbfd"
    char * p = strchr(str, ' ');
    if (p == NULL)
    {
        return;
    }

    lw_config_t* config = _lw_get_config();
    if (!config)
        return;

    uint8_t end_pos_word = p - str + 1;
    p = skip_space(p);
    if (strncmp(str, "dev-eui", end_pos_word-1) == 0)
    {
        memset(config->dev_eui, 0, LW_DEV_EUI_LEN);
        strncpy(config->dev_eui, p, strlen(p));
        _lw_reload_config();
    }
    else if (strncmp(str, "app-key", end_pos_word-1) == 0)
    {
        memset(config->app_key, 0, LW_DEV_EUI_LEN);
        strncpy(config->app_key, p, strlen(p));
        _lw_reload_config();
    }
}


bool comms_get_id(char* str, uint8_t len)
{
    if (len < LW_DEV_EUI_LEN + 1)
        return false;
    lw_config_t* config = _lw_get_config();
    if (!config)
        return false;
    strncpy(str, config->dev_eui, LW_DEV_EUI_LEN);
    str[LW_DEV_EUI_LEN] = 0;
    return true;
}
