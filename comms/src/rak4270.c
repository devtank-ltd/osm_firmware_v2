#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "uart_rings.h"
#include "uarts.h"
#include "config.h"
#include "rak4270.h"
#include "log.h"
#include "cmd.h"
#include "measurements.h"
#include "persist_config.h"
#include "common.h"
#include "timers.h"
#include "update.h"
#include "pinmap.h"
#include "lw.h"

#define RAK4270_HEADER_SIZE                      17
#define RAK4270_TAIL_SIZE                        2

#define RAK4270_PAYLOAD_MAX_DEFAULT              242


_Static_assert (PROTOCOL_HEX_ARRAY_SIZE * 2 < RAK4270_PAYLOAD_MAX_DEFAULT, "Measurement send data max longer than LoRaWAN payload max.");

/* AT commands https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4270-Module/AT-Command-Manual */

#define RAK4270_BUFFER_SIZE                  UART_1_OUT_BUF_SIZE
#define RAK4270_CONFIG_TIMEOUT_S             30
#define RAK4270_RESET_GPIO_DEFAULT_MS        10
#define RAK4270_SLOW_RESET_TIMEOUT_MINS      15
#define RAK4270_MAX_RESEND                   5
#define RAK4270_DELAY_MS                     2

#define RAK4270_ERROR_PREFIX                 "ERROR: "

#define RAK4270_SETTING_JOIN_MODE_OTAA       0
#define RAK4270_SETTING_JOIN_MODE_ABP        1
#define RAK4270_SETTING_CLASS_A              0
#define RAK4270_SETTING_CLASS_C              2
#define RAK4270_SETTING_UNCONFIRM            0
#define RAK4270_SETTING_CONFIRM              1
#define RAK4270_SETTING_ADR_DISABLED         0
#define RAK4270_SETTING_ADR_ENABLED          1
#define RAK4270_SETTING_DR_0                 0
#define RAK4270_SETTING_DR_1                 1
#define RAK4270_SETTING_DR_2                 2
#define RAK4270_SETTING_DR_3                 3
#define RAK4270_SETTING_DR_4                 4
#define RAK4270_SETTING_DR_5                 5
#define RAK4270_SETTING_DR_6                 6
#define RAK4270_SETTING_DR_7                 7
#define RAK4270_SETTING_DUTY_CYCLE_DISABLED  0
#define RAK4270_SETTING_DUTY_CYCLE_ENABLED   1

#define RAK4270_CONFIG_JOIN_MODE             STR(RAK4270_SETTING_JOIN_MODE_OTAA)
#define RAK4270_CONFIG_CLASS                 STR(RAK4270_SETTING_CLASS_C)
#define RAK4270_CONFIG_CONFIRM_TYPE          STR(RAK4270_SETTING_CONFIRM)
#define RAK4270_CONFIG_ADR                   STR(RAK4270_SETTING_ADR_DISABLED)
#define RAK4270_CONFIG_DR                    STR(RAK4270_SETTING_DR_4)
#define RAK4270_CONFIG_DUTY_CYCLE            STR(RAK4270_SETTING_DUTY_CYCLE_DISABLED)

#define RAK4270_REGION_NAME_AS923            "AS923"

typedef enum
{
    RAK4270_STATE_OFF,
    RAK4270_STATE_WAIT_INIT,
    RAK4270_STATE_WAIT_INIT_OK,
    RAK4270_STATE_WAIT_REINIT,
    RAK4270_STATE_WAIT_CONN,
    RAK4270_STATE_WAIT_OK,
    RAK4270_STATE_WAIT_ACK,
    RAK4270_STATE_IDLE,
    RAK4270_STATE_WAIT_UCONF_OK,
    RAK4270_STATE_WAIT_CODE_OK,
    RAK4270_STATE_WAIT_CONF_OK,
    RAK4270_STATE_UNCONFIGURED,
} rak4270_states_t;


typedef enum
{
    RAK4270_RECV_ACK,
    RAK4270_RECV_DATA,
    RAK4270_RECV_ERR_NOT_START,
    RAK4270_RECV_ERR_BAD_FMT,
    RAK4270_RECV_ERR_UNFIN,
} rak4270_recv_packet_types_t;


typedef enum
{
    RAK4270_ERROR_CMD_UNSUPP         =  1,   // reset
    RAK4270_ERROR_INVALID_CMD        =  2,   // reset
    RAK4270_ERROR_R_W_FLASH          =  3,   // reset
    RAK4270_ERROR_UART_ERROR         =  5,   // reset
    RAK4270_ERROR_TRANS_BUSY         =  80,  //  retry
    RAK4270_ERROR_SVC_UNKNOWN        =  81,  // reset
    RAK4270_ERROR_INVLD_LORA_PARA    =  82,  // reset
    RAK4270_ERROR_INVLD_LORA_FREQ    =  83,  // reset
    RAK4270_ERROR_INVLD_LORA_DR      =  84,  // reset
    RAK4270_ERROR_INVLD_LORA_FR_DR   =  85,  // reset
    RAK4270_ERROR_NOT_CONNECTED      =  86,  // reset
    RAK4270_ERROR_PACKET_SIZE        =  87,  //  reduce limit, reset, drop data
    RAK4270_ERROR_SVC_CLOSED         =  88,  // reset
    RAK4270_ERROR_REGION_UNSUPP      =  89,  // reset
    RAK4270_ERROR_DUTY_CYCLE_CLSD    =  90,  // reset
    RAK4270_ERROR_CHAN_NO_VLD_FND    =  91,  // reset
    RAK4270_ERROR_CHAN_NO_AVLBL_FND  =  92,  // reset
    RAK4270_ERROR_STATUS_ERROR       =  93,  // reset
    RAK4270_ERROR_TIMEOUT_LORA_TRANS =  94,  // reset
    RAK4270_ERROR_TIMEOUT_RX1        =  95,  //  nothing
    RAK4270_ERROR_TIMEOUT_RX2        =  96,  //  retry
    RAK4270_ERROR_RECV_RX1           =  97,  //  retry
    RAK4270_ERROR_RECV_RX2           =  98,  //  retry
    RAK4270_ERROR_JOIN_FAIL          =  99,  // reset
    RAK4270_ERROR_DUP_DOWNLINK       = 100,  //  nothing
    RAK4270_ERROR_PAYLOAD_SIZE       = 101,  //  reduce limit, reset, drop data
    RAK4270_ERROR_MANY_PKTS_LOST     = 102,  // reset
    RAK4270_ERROR_ADDR_FAIL          = 103,  // reset
    RAK4270_ERROR_INVLD_MIC          = 104,  //  nothing
} rak4270_error_codes_t;


typedef enum
{
    RAK4270_BKUP_MSG_BLANK,
    RAK4270_BKUP_MSG_STR,
    RAK4270_BKUP_MSG_HEX,
} rak4270_backup_msg_types_t;


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
} rak4270_payload_t;


typedef struct
{
    rak4270_states_t state;
    unsigned    init_step;
    uint32_t    last_message_time;
    uint8_t     resend_count;
    uint8_t     reset_count;
} rak4270_state_machine_t;


typedef char rak4270_msg_buf_t[64];


typedef struct
{
    rak4270_backup_msg_types_t backup_type;
    union
    {
        char    string[RAK4270_BUFFER_SIZE];
        struct
        {
            uint8_t len;
            int8_t arr[PROTOCOL_HEX_ARRAY_SIZE];
        } hex;
    };
} rak4270_backup_msg_t;


typedef struct
{
    uint8_t                 code;
    bool                    valid;
} error_code_t;


static rak4270_state_machine_t   _rak4270_state_machine                   = {.state=RAK4270_STATE_OFF, .init_step=0, .last_message_time=0, .resend_count=0, .reset_count=0};
static port_n_pins_t        _rak4270_reset_gpio                      = COMMS_RESET_PORT_N_PINS;
static char                 _rak4270_out_buffer[RAK4270_BUFFER_SIZE]      = {0};
static uint8_t              _rak4270_port                            = 0;
static uint32_t             _rak4270_chip_off_time                   = 0;
static uint32_t             _rak4270_reset_timeout                   = RAK4270_RESET_GPIO_DEFAULT_MS;
static rak4270_backup_msg_t      _rak4270_backup_message                  = {.backup_type=RAK4270_BKUP_MSG_BLANK, .hex={.len=0, .arr={0}}};
static error_code_t         _rak4270_error_code                      = {0, false};
static char                 _rak4270_cmd_ascii[CMD_LINELEN]          = "";
static uint16_t             _next_fw_chunk_id                   = 0;

static uint16_t             _rak4270_packet_max_size                  = RAK4270_PAYLOAD_MAX_DEFAULT;

static rak4270_msg_buf_t _init_msgs[] = { "at+set_config=lora:default_parameters",
                                     "at+set_config=lora:join_mode:"RAK4270_CONFIG_JOIN_MODE,
                                     "at+set_config=lora:class:"RAK4270_CONFIG_CLASS,
                                     "at+set_config=lora:confirm:"RAK4270_CONFIG_CONFIRM_TYPE,
                                     "at+set_config=lora:adr:"RAK4270_CONFIG_ADR,
                                     "at+set_config=lora:dr:"RAK4270_CONFIG_DR,
                                     "at+set_config=lora:dutycycle_enable:"RAK4270_CONFIG_DUTY_CYCLE,
                                     "REGION goes here",
                                     "Dev EUI goes here",
                                     "App EUI goes here",
                                     "App Key goes here"};


uint16_t rak4270_get_mtu(void)
{
    return _rak4270_packet_max_size;
}


static void _rak4270_chip_off(void)
{
    _rak4270_state_machine.state = RAK4270_STATE_OFF;
    gpio_clear(_rak4270_reset_gpio.port, _rak4270_reset_gpio.pins);
}


static void _rak4270_msg_delay(void)
{
    spin_blocking_ms(RAK4270_DELAY_MS);
}


static void _rak4270_chip_on(void)
{
    _rak4270_state_machine.state = RAK4270_STATE_WAIT_INIT;
    _rak4270_state_machine.init_step = 0;
    gpio_set(_rak4270_reset_gpio.port, _rak4270_reset_gpio.pins);
}


static void _rak4270_update_last_message_time(void)
{
    _rak4270_state_machine.last_message_time = get_since_boot_ms();
}


static unsigned _rak4270_write_to_uart(char* string)
{
    _rak4270_update_last_message_time();
    return uart_ring_out(COMMS_UART, string, strlen(string));
}


static void _rak4270_write(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(_rak4270_out_buffer, RAK4270_BUFFER_SIZE, fmt, args);
    va_end(args);
    comms_debug("<< %s", _rak4270_out_buffer);
    size_t len = strlen(_rak4270_out_buffer);
    _rak4270_out_buffer[len] = '\r';
    _rak4270_out_buffer[len+1] = '\n';
    _rak4270_out_buffer[len+2] = 0;
    _rak4270_write_to_uart(_rak4270_out_buffer);
}


static uint8_t _rak4270_get_port(void)
{
    _rak4270_port++;
    if (_rak4270_port > 223)
    {
        _rak4270_port = 0;
    }
    return _rak4270_port;
}


static void _rak4270_reset_gpio_init(void)
{
    rcc_periph_clock_enable(PORT_TO_RCC(_rak4270_reset_gpio.port));
    gpio_mode_setup(_rak4270_reset_gpio.port,
                    GPIO_MODE_OUTPUT,
                    GPIO_PUPD_NONE,
                    _rak4270_reset_gpio.pins);
}


static const char* _rak4270_region_name(uint8_t region)
{
    const char* name;
    switch (region)
    {
        case LW_REGION_EU433:
            name = LW_REGION_NAME_EU433;
            break;
        case LW_REGION_CN470:
            name = LW_REGION_NAME_CN470;
            break;
        case LW_REGION_IN865:
            name = LW_REGION_NAME_IN865;
            break;
        case LW_REGION_EU868:
            name = LW_REGION_NAME_EU868;
            break;
        case LW_REGION_US915:
            name = LW_REGION_NAME_US915;
            break;
        case LW_REGION_AU915:
            name = LW_REGION_NAME_AU915;
            break;
        case LW_REGION_KR920:
            name = LW_REGION_NAME_KR920;
            break;
        case LW_REGION_AS923_2:
            /* fall through */
        case LW_REGION_AS923_3:
            /* fall through */
        case LW_REGION_AS923_4:
            comms_debug("Not capable of set region, defaulting to LW_REGION_AS923_1");
            /* fall through */
        case LW_REGION_AS923_1:
            /* Different from default */
            name = RAK4270_REGION_NAME_AS923;
            break;
        case LW_REGION_RU864:
            comms_debug("Not capable of set region.");
            /* fall through */
        default:
            name = NULL;
            break;
    }
    return name;
}


static bool _rak4270_load_config(void)
{
    if (!lw_persist_data_is_valid())
    {
        log_error("No LoRaWAN Dev EUI and/or App Key.");
        return false;
    }

    lw_config_t* config = lw_get_config();
    if (!config)
        return false;
    const char* region_name = _rak4270_region_name(config->region);
    if (!region_name)
    {
        log_error("Invalid region, setting to EU868.");
        region_name = _rak4270_region_name(LW_REGION_EU868);
    }

    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-4], sizeof(rak4270_msg_buf_t), "at+set_config=lora:region:%.*s",  LW_REGION_LEN,  region_name    );
    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-3], sizeof(rak4270_msg_buf_t), "at+set_config=lora:dev_eui:%.*s", LW_DEV_EUI_LEN, config->dev_eui);
    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-2], sizeof(rak4270_msg_buf_t), "at+set_config=lora:app_eui:%.*s", LW_DEV_EUI_LEN, config->dev_eui);
    snprintf(_init_msgs[ARRAY_SIZE(_init_msgs)-1], sizeof(rak4270_msg_buf_t), "at+set_config=lora:app_key:%.*s", LW_APP_KEY_LEN, config->app_key);
    return true;
}


static void _rak4270_chip_init(void)
{
    if (_rak4270_state_machine.state != RAK4270_STATE_OFF)
    {
        comms_debug("Chip already on, restarting chip.");
        _rak4270_chip_off();
        timer_delay_us_64(RAK4270_RESET_GPIO_DEFAULT_MS * 1000);
    }
    comms_debug("Initialising LoRaWAN chip.");
    _rak4270_chip_on();
}


static void _rak4270_set_confirmed(bool confirmed)
{
    if (confirmed)
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_CONF_OK;
        comms_debug("Setting to confirmed.");
        _rak4270_write("at+set_config=lora:confirm:1");
    }
    else
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_UCONF_OK;
        comms_debug("Setting to unconfirmed.");
        _rak4270_write("at+set_config=lora:confirm:0");
    }
}


static void _rak4270_join_network(void)
{
    _rak4270_write("at+join");
}


static void _rak4270_soft_reset(void)
{
    _rak4270_write("at+set_config=device:restart");
}


bool rak4270_send_ready(void)
{
    return _rak4270_state_machine.state == RAK4270_STATE_IDLE;
}


bool rak4270_send_str(char* str)
{
    if (!rak4270_send_ready())
    {
        comms_debug("Cannot send '%s' as chip is not in IDLE state.", str);
        return false;
    }
    _rak4270_state_machine.state = RAK4270_STATE_WAIT_OK;
    _rak4270_write("at+send=lora:%u:%s", _rak4270_get_port(), str);
    _rak4270_backup_message.backup_type = RAK4270_BKUP_MSG_STR;
    strncpy(_rak4270_backup_message.string, str, strlen(str));
    return true;
}


static void _rak4270_clear_backup(void)
{
    _rak4270_backup_message.backup_type = RAK4270_BKUP_MSG_BLANK;
}


static void _rak4270_retry_write(void)
{
    if (_rak4270_state_machine.resend_count >= RAK4270_MAX_RESEND)
    {
        comms_debug("Failed to successfully resend (%u times), resetting chip.", RAK4270_MAX_RESEND);
        _rak4270_state_machine.resend_count = 0;
        rak4270_reset();
        on_comms_sent_ack(false);
        return;
    }
    _rak4270_state_machine.resend_count++;
    comms_debug("Resending %"PRIu8"/%u", _rak4270_state_machine.resend_count, RAK4270_MAX_RESEND);
    switch (_rak4270_backup_message.backup_type)
    {
        case RAK4270_BKUP_MSG_STR:
            _rak4270_state_machine.state = RAK4270_STATE_IDLE;
            rak4270_send_str(_rak4270_backup_message.string);
            break;
        case RAK4270_BKUP_MSG_HEX:
            _rak4270_state_machine.state = RAK4270_STATE_IDLE;
            rak4270_send(_rak4270_backup_message.hex.arr, _rak4270_backup_message.hex.len);
            break;
        default:
            comms_debug("Broken backup, cant resend");
            return;
    }
}


 void _rak4270_send_alive(void)
{
    comms_debug("Sending an 'is alive' packet.");
    _rak4270_write("at+send=lora:%"PRIu8":", _rak4270_get_port());
    return;
}


void rak4270_init(void)
{
    lw_persist_data_is_valid();

    _rak4270_reset_gpio_init();
    if (_rak4270_state_machine.state != RAK4270_STATE_OFF)
    {
        log_error("LoRaWAN chip not in DISCONNECTED state.");
        return;
    }
    if (!_rak4270_load_config())
    {
        log_error("Loading LoRaWAN config failed. Not connecting to network.");
        _rak4270_state_machine.state = RAK4270_STATE_UNCONFIGURED;
        return;
    }
    _rak4270_chip_init();
}


static unsigned _rak4270_handle_unsol_2_rak4270_cmd_ascii(char *p)
{
    char* rak4270_p = _rak4270_cmd_ascii;
    char* rak4270_p_last = rak4270_p + CMD_LINELEN - 1;

    while(*p && rak4270_p < rak4270_p_last)
    {
        uint8_t val = lw_consume(p, 2);
        p+=2;
        if (val != 0)
            *rak4270_p++ = (char)val;
        else
            break;
    }
    *rak4270_p = 0;
    return (uintptr_t)rak4270_p - (uintptr_t)_rak4270_cmd_ascii;
}


bool rak4270_send_allowed(void)
{
    // TODO: This function could probably be done better.
    return (_next_fw_chunk_id != 0);
}


static void _rak4270_handle_unsol(rak4270_payload_t * incoming_pl)
{
    char* p = incoming_pl->data;

    unsigned len = strlen(p);

    if (len % 1 || len < 10)
    {
        log_error("Invalid RAK4270 unsol msg");
        return;
    }

    if (lw_consume(p, 2) != LW_UNSOL_VERSION)
    {
        log_error("Couldn't parse RAK4270 unsol msg");
        return;
    }
    p += 2;
    uint32_t pl_id = (uint32_t)lw_consume(p, 8);
    p += 8;

    switch (pl_id)
    {
        case LW_ID_CMD:
        {
            unsigned cmd_len = _rak4270_handle_unsol_2_rak4270_cmd_ascii(p);
            cmds_process(_rak4270_cmd_ascii, cmd_len);
            break;
        }
        case LW_ID_CCMD:
        {
            unsigned cmd_len = _rak4270_handle_unsol_2_rak4270_cmd_ascii(p);
            _rak4270_error_code.code = cmds_process(_rak4270_cmd_ascii, cmd_len); /* command_resp_t */
            _rak4270_error_code.valid = true;
            break;
        }
        case LW_ID_FW_START:
        {
            uint16_t count = (uint16_t)lw_consume(p, 4);
            comms_debug("FW of %"PRIu16" chunks", count);
            _next_fw_chunk_id = 0;
            fw_ota_reset();
            break;
        }
        case LW_ID_FW_CHUNK:
        {
            uint16_t chunk_id = (uint16_t)lw_consume(p, 4);
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
                uint8_t b = (uint8_t)lw_consume(p, 2);
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
                log_error("RAK4270 FW Finish invalid");
                return;
            }
            uint16_t crc = (uint16_t)lw_consume(p, 4);
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


static rak4270_recv_packet_types_t _rak4270_parse_recv(char* message, rak4270_payload_t* payload)
{
    // at+recv=PORT,RSSI,SNR,DATALEN:DATA
    const char recv_msg[] = "at+recv=";
    char* pos = NULL;
    char* next_pos = NULL;

    if (strncmp(message, recv_msg, strlen(recv_msg)) != 0)
    {
        return RAK4270_RECV_ERR_NOT_START;
    }

    pos = message + strlen(recv_msg);
    for (int i = 0; i < 3; i++)
    {
        payload->header.raw[i] = strtol(pos, &next_pos, 10);
        if ((*next_pos) != ',')
        {
            return RAK4270_RECV_ERR_BAD_FMT;
        }
        pos = next_pos + 1;
    }
    payload->header.datalen = strtol(pos, &next_pos, 10);
    if (*next_pos == '\0')
    {
        payload->data = NULL;
        if (payload->header.datalen == 0)
        {
            return RAK4270_RECV_ACK;
        }
        return RAK4270_RECV_ERR_BAD_FMT;
    }
    if (*next_pos == ':')
    {
        payload->data = next_pos + 1;
        int size_diff = strlen(payload->data)/2 - (int)payload->header.datalen;
        if (size_diff != 0)
        {
            return RAK4270_RECV_ERR_BAD_FMT;
        }
        return RAK4270_RECV_DATA;
    }
    return RAK4270_RECV_ERR_BAD_FMT;
}


static bool _rak4270_msg_is_error(char* message)
{
    return msg_is("ERROR:", message);
}


static bool _rak4270_msg_is_initialisation(char* message)
{
    return msg_is("Initialization OK", message);
}


static bool _rak4270_msg_is_ok(char* message)
{
    return msg_is("OK", message);
}


static bool _rak4270_msg_is_connected(char* message)
{
    return msg_is("OK Join Success", message);
}


static bool _rak4270_msg_is_ack(char* message)
{
    rak4270_payload_t payload;
    return (_rak4270_parse_recv(message, &payload) == RAK4270_RECV_ACK);
}


static bool _rak4270_write_next_init_step(void)
{
    if (_rak4270_state_machine.init_step >= ARRAY_SIZE(_init_msgs))
    {
        return false;
    }
    _rak4270_write(_init_msgs[_rak4270_state_machine.init_step++]);
    return true;
}


static void _rak4270_send_error_code(void)
{
    // FIXME: No real protocol or anything is set up with this yet.
    if (_rak4270_error_code.valid)
    {
        char err_msg[11] = "";
        snprintf(err_msg, 11, "%"PRIx16, _rak4270_error_code.code);
        comms_debug("Sending error message '%s'", err_msg);
        rak4270_send_str(err_msg);
        _rak4270_error_code.valid = false;
    }
}


void rak4270_reset(void)
{
    if (_rak4270_state_machine.state == RAK4270_STATE_OFF)
    {
        comms_debug("Already resetting.");
        return;
    }
    _rak4270_chip_off();
    _rak4270_chip_off_time = get_since_boot_ms();
    if (++_rak4270_state_machine.reset_count > 2)
    {
        comms_debug("Going into long reset mode (wait %"PRIi16" mins).", RAK4270_SLOW_RESET_TIMEOUT_MINS);
        _rak4270_reset_timeout = RAK4270_SLOW_RESET_TIMEOUT_MINS * 60 * 1000;
    }
    else
    {
        _rak4270_reset_timeout = 10;
    }
}


static bool _rak4270_reload_config(void)
{
    if (!_rak4270_load_config())
    {
        return false;
    }
    _rak4270_state_machine.reset_count = 0;
    _rak4270_state_machine.state = RAK4270_STATE_OFF;
    rak4270_reset();
    return true;
}


static void _rak4270_handle_error(char* message)
{
    char* pos = NULL;
    char* next_pos = NULL;
    rak4270_error_codes_t err_no = 0;

    pos = message + strlen(RAK4270_ERROR_PREFIX);
    err_no = (rak4270_error_codes_t)strtol(pos, &next_pos, 10);
    switch (err_no)
    {
        case RAK4270_ERROR_TRANS_BUSY:
            comms_debug("LoRa Transceiver is busy, retrying.");
            _rak4270_retry_write();
            break;
        case RAK4270_ERROR_PACKET_SIZE:
            comms_debug("Packet size too large, reducing limit throwing data and resetting chip.");
            _rak4270_packet_max_size -= 2;
            on_comms_sent_ack(false);
            rak4270_reset();
            break;
        case RAK4270_ERROR_TIMEOUT_RX1:
            if (rak4270_get_connected())
            {
                comms_debug("Timed out waiting for packet in windox RX1.");
                _rak4270_retry_write();
            }
            else
            {
                comms_debug("Send alive wasn't responded to.");
                rak4270_reset();
            }
            break;
        case RAK4270_ERROR_TIMEOUT_RX2:
            if (rak4270_get_connected())
            {
                comms_debug("Timed out waiting for packet in windox RX2.");
                _rak4270_retry_write();
            }
            else
            {
                comms_debug("Send alive wasn't responded to.");
                rak4270_reset();
            }
            break;
        case RAK4270_ERROR_RECV_RX1:
            comms_debug("Error receiving message in RX1.");
            _rak4270_retry_write();
            break;
        case RAK4270_ERROR_RECV_RX2:
            comms_debug("Error receiving message in RX2.");
            _rak4270_retry_write();
            break;
        case RAK4270_ERROR_DUP_DOWNLINK:
            comms_debug("Duplicate downlink.");
            break;
        case RAK4270_ERROR_PAYLOAD_SIZE:
            comms_debug("Packet size not valid for current data rate, reducing limit throwing data and resetting chip.");
            _rak4270_packet_max_size -= 2;
            on_comms_sent_ack(false);
            rak4270_reset();
            break;
        case RAK4270_ERROR_INVLD_MIC:
            comms_debug("Invalid MIC in LoRa message.");
            break;
        default:
            comms_debug("Error Code %"PRIu8", resetting chip.", err_no);
            rak4270_reset();
            break;
    }
}


static void _rak4270_process_wait_init(char* message)
{
    if (_rak4270_msg_is_initialisation(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_INIT_OK;
        _rak4270_write_next_init_step();
    }
}


static void _rak4270_process_wait_init_ok(char* message)
{
    if (_rak4270_msg_is_ok(message))
    {
        _rak4270_msg_delay();
        if (_rak4270_state_machine.init_step == ARRAY_SIZE(_init_msgs))
        {
            _rak4270_state_machine.state = RAK4270_STATE_WAIT_REINIT;
            _rak4270_soft_reset();
        }
        else
        {
            _rak4270_write_next_init_step();
        }
    }
}


static void _rak4270_process_wait_reinit(char* message)
{
    if (_rak4270_msg_is_initialisation(message))
    {
        _rak4270_msg_delay();
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_CONN;
        _rak4270_join_network();
    }
}


static void _rak4270_process_wait_conn(char* message)
{
    if (_rak4270_msg_is_connected(message))
    {
        _rak4270_msg_delay();
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_OK;
        _rak4270_send_alive();
    }
}


static void _rak4270_process_wait_ok(char* message)
{
    if (_rak4270_msg_is_ok(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_ACK;
    }
}


static void _rak4270_process_wait_ack(char* message)
{
    if (_rak4270_msg_is_ack(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_IDLE;
        _rak4270_state_machine.reset_count = 0;
        _rak4270_state_machine.resend_count = 0;
        _rak4270_clear_backup();
        on_comms_sent_ack(true);
    }
}


static void _rak4270_process_wait_uconf_ok(char* message)
{
    if (_rak4270_msg_is_ok(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_CODE_OK;
        _rak4270_send_error_code();
    }
}


static void _rak4270_process_wait_code_ok(char* message)
{
    if (_rak4270_msg_is_ok(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_CONF_OK;
        _rak4270_set_confirmed(true);
    }
}


static void _rak4270_process_wait_conf_ok(char* message)
{
    if (_rak4270_msg_is_ok(message))
    {
        _rak4270_state_machine.state = RAK4270_STATE_IDLE;
    }
}


void rak4270_process(char* message)
{
    _rak4270_update_last_message_time();
    if (_rak4270_msg_is_error(message))
    {
        _rak4270_handle_error(message);
        return;
    }

    rak4270_payload_t payload;
    rak4270_recv_packet_types_t recv_type = _rak4270_parse_recv(message, &payload);

    if (recv_type == RAK4270_RECV_ERR_BAD_FMT)
    {
        log_error("Invalid recv");
        return;
    }

    if (recv_type == RAK4270_RECV_DATA)
    {
        _rak4270_handle_unsol(&payload);
        return;
    }

    if (recv_type == RAK4270_RECV_ACK &&
        _rak4270_state_machine.state != RAK4270_STATE_WAIT_ACK)
    {
        comms_debug("Unexpected ACK");
        return;
    }

    switch(_rak4270_state_machine.state)
    {
        case RAK4270_STATE_WAIT_INIT:
            _rak4270_process_wait_init(message);
            break;
        case RAK4270_STATE_WAIT_INIT_OK:
            _rak4270_process_wait_init_ok(message);
            break;
        case RAK4270_STATE_WAIT_REINIT:
            _rak4270_process_wait_reinit(message);
            break;
        case RAK4270_STATE_WAIT_CONN:
            _rak4270_process_wait_conn(message);
            break;
        case RAK4270_STATE_WAIT_OK:
            _rak4270_process_wait_ok(message);
            break;
        case RAK4270_STATE_WAIT_ACK:
            _rak4270_process_wait_ack(message);
            break;
        case RAK4270_STATE_IDLE:
            break;
        case RAK4270_STATE_WAIT_UCONF_OK:
            _rak4270_process_wait_uconf_ok(message);
            break;
        case RAK4270_STATE_WAIT_CODE_OK:
            _rak4270_process_wait_code_ok(message);
            break;
        case RAK4270_STATE_WAIT_CONF_OK:
            _rak4270_process_wait_conf_ok(message);
            break;
        case RAK4270_STATE_UNCONFIGURED:
            /* Also panic? */
            break;
        case RAK4270_STATE_OFF:
            /* More panic */
            break;
    }
}


static unsigned _rak4270_send_size(uint16_t arr_len)
{
    unsigned next_rak4270_port = _rak4270_port;
    if (next_rak4270_port > 223)
        next_rak4270_port = 0;

    unsigned size = (RAK4270_HEADER_SIZE + (arr_len * 2) + RAK4270_TAIL_SIZE);

    if (next_rak4270_port < 99)
    {
        size--;
        if (next_rak4270_port < 9)
            size--;
    }

    return size;
}


void rak4270_send(int8_t* hex_arr, uint16_t arr_len)
{
    if (_rak4270_state_machine.state == RAK4270_STATE_IDLE)
    {
        char header_str[RAK4270_HEADER_SIZE + 1] = {0};
        char hex_str[3] = {0};
        unsigned expected = _rak4270_send_size(arr_len);
        unsigned header_size = snprintf(header_str, sizeof(header_str), "at+send=lora:%"PRIu8":", _rak4270_get_port());
        unsigned sent = 0;
        _rak4270_msg_delay();
        sent += _rak4270_write_to_uart(header_str);
        const char desc[]  ="LORA >> ";
        uart_ring_out(CMD_UART, desc, strlen(desc));
        uart_ring_out(CMD_UART, header_str, header_size);
        for (uint16_t i = 0; i < arr_len; i++)
        {
            snprintf(hex_str, 3, "%02"PRIx8, hex_arr[i]);
            sent += _rak4270_write_to_uart(hex_str);
            uart_ring_out(CMD_UART, hex_str, 2);
        }
        sent += _rak4270_write_to_uart("\r\n");
        uart_ring_out(CMD_UART, "\r\n", 2);
        _rak4270_state_machine.state = RAK4270_STATE_WAIT_OK;

        if (_rak4270_backup_message.hex.arr != hex_arr)
        {
            _rak4270_backup_message.backup_type = RAK4270_BKUP_MSG_HEX;
            _rak4270_backup_message.hex.len = arr_len;
            memcpy(_rak4270_backup_message.hex.arr, hex_arr, arr_len);
        }

        if (sent != expected)
            log_error("Failed to send all bytes over LoRaWAN (%u != %u)", sent, expected);
        else
            comms_debug("Sent %u bytes", sent);
    }
    else
    {
        comms_debug("Incorrect state to send : %u", (unsigned)_rak4270_state_machine.state);
    }
}


bool rak4270_get_connected(void)
{
    return (_rak4270_state_machine.state == RAK4270_STATE_WAIT_OK       ||
            _rak4270_state_machine.state == RAK4270_STATE_WAIT_ACK      ||
            _rak4270_state_machine.state == RAK4270_STATE_IDLE          ||
            _rak4270_state_machine.state == RAK4270_STATE_WAIT_UCONF_OK ||
            _rak4270_state_machine.state == RAK4270_STATE_WAIT_CODE_OK  ||
            _rak4270_state_machine.state == RAK4270_STATE_WAIT_CONF_OK  );
}


void rak4270_loop_iteration(void)
{
    uint32_t now = get_since_boot_ms();
    switch(_rak4270_state_machine.state)
    {
        case RAK4270_STATE_OFF:
            if (since_boot_delta(now, _rak4270_chip_off_time) > _rak4270_reset_timeout)
            {
                if (lw_persist_data_is_valid())
                    _rak4270_chip_on();
                else
                    rak4270_reset();
            }
            break;
        case RAK4270_STATE_IDLE:
            break;
        case RAK4270_STATE_UNCONFIGURED:
            break;
        default:
            if (since_boot_delta(get_since_boot_ms(), _rak4270_state_machine.last_message_time) > RAK4270_CONFIG_TIMEOUT_S * 1000)
            {
                if (_rak4270_state_machine.state == RAK4270_STATE_WAIT_OK || _rak4270_state_machine.state == RAK4270_STATE_WAIT_ACK)
                {
                    on_comms_sent_ack(false);
                }
                comms_debug("LoRa chip timed out, resetting.");
                rak4270_reset();
            }
            break;
    }
}


static command_response_t _rak4270_conn(char* str)
{
    if (rak4270_get_connected())
    {
        comms_debug("1 | Connected");
        return COMMAND_RESP_OK;
    }
    comms_debug("0 | Disconnected");
    return COMMAND_RESP_ERR;
}


static command_response_t _rak4270_config_setup_str(char* str)
{
    if (lw_config_setup_str(str))
    {
        _rak4270_reload_config();
        return COMMAND_RESP_OK;
    }
    return COMMAND_RESP_ERR;
}


bool rak4270_get_id(char* str, uint8_t len)
{
    return lw_get_id(str, len);
}


struct cmd_link_t* rak4270_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_config", "Set the comms config",        _rak4270_config_setup_str      , false , NULL },
        { "comms_conn",   "Get if connected or not",     _rak4270_conn                  , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


void rak4270_power_down(void)
{
    _rak4270_chip_off();
}


/* Return false if different
 *        true  if same      */
bool rak4270_persist_config_cmp(void* d0, void* d1)
{
    return lw_persist_config_cmp(
        (lw_config_t*)((comms_config_t*)d0)->setup,
        (lw_config_t*)((comms_config_t*)d1)->setup);
}
