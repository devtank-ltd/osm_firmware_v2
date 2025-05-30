#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <osm/comms/at_wifi.h>
#include <osm/core/uart_rings.h>
#include "pinmap.h"
#include <osm/core/common.h>
#include <osm/core/log.h>
#include <osm/core/cmd.h>
#include <osm/protocols/protocol.h>
#include <osm/core/platform.h>


#define COMMS_ID_STR                            "WIFI"


#define AT_WIFI_TIMEOUT_MS_WIFI                 30000
#define AT_WIFI_TIMEOUT_MS_MQTT                 30000
#define AT_WIFI_TIMEOUT_MS_DEFAULT              30000

#define AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS    60000
#define AT_WIFI_MQTT_FAIL_CONNECT_TIMEOUT_MS    60000

#define AT_WIFI_STILL_OFF_TIMEOUT          10000
#define AT_WIFI_LIST_TIMEOUT                5000


#define AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_SSID                "    \"WIFI SSID\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_PWD                 "    \"WIFI PWD\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_COUNTRY_CODE             "    \"COUNTRY CODE\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_CHANNEL_START            "    \"CHANNEL START\": %"PRIu16","
#define AT_WIFI_PRINT_CFG_JSON_FMT_CHANNEL_COUNT            "    \"CHANNEL COUNT\": %"PRIu16","

#define AT_WIFI_PRINT_CFG_JSON_WIFI_SSID(_wifi_ssid)        AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_SSID    , AT_WIFI_MAX_SSID_LEN          ,_wifi_ssid
#define AT_WIFI_PRINT_CFG_JSON_WIFI_PWD(_wifi_pwd)          AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_PWD     , AT_WIFI_MAX_PWD_LEN           , _wifi_pwd
#define AT_WIFI_PRINT_CFG_JSON_COUNTRY_CODE(_country_code)  AT_WIFI_PRINT_CFG_JSON_FMT_COUNTRY_CODE , AT_WIFI_MAX_COUNTRY_CODE_LEN  , _country_code
#define AT_WIFI_PRINT_CFG_JSON_CHANNEL_START(_schan)        AT_WIFI_PRINT_CFG_JSON_FMT_CHANNEL_START, _schan
#define AT_WIFI_PRINT_CFG_JSON_CHANNEL_COUNT(_nchan)        AT_WIFI_PRINT_CFG_JSON_FMT_CHANNEL_COUNT, _nchan

#define AT_WIFI_AP_LIST_LEN                     5


enum at_wifi_states_t
{
    AT_WIFI_STATE_OFF,
    AT_WIFI_STATE_IS_CONNECTED,
    AT_WIFI_STATE_RESTORE,
    AT_WIFI_STATE_DISABLE_ECHO,
    AT_WIFI_STATE_WAIT_MAC_ADDRESS,
    AT_WIFI_STATE_NO_CONF,
    AT_WIFI_STATE_RF_REGION,
    AT_WIFI_STATE_WIFI_SETTING_MODE,
    AT_WIFI_STATE_WIFI_INIT,
    AT_WIFI_STATE_WIFI_CONNECTING,
    AT_WIFI_STATE_SNTP_WAIT_SET,
    AT_WIFI_STATE_MQTT_WAIT_USR_CONF,
    AT_WIFI_STATE_MQTT_WAIT_CONF,
    AT_WIFI_STATE_MQTT_CONNECTING,
    AT_WIFI_STATE_MQTT_WAIT_SUB,
    AT_WIFI_STATE_MQTT_IS_CONNECTED,
    AT_WIFI_STATE_MQTT_IS_SUBSCRIBED,
    AT_WIFI_STATE_IDLE,
    AT_WIFI_STATE_MQTT_WAIT_PUB,
    AT_WIFI_STATE_MQTT_PUBLISHING,
    AT_WIFI_STATE_MQTT_FAIL_CONNECT,
    AT_WIFI_STATE_WIFI_FAIL_CONNECT,
    AT_WIFI_STATE_WAIT_WIFI_STATE,
    AT_WIFI_STATE_WAIT_MQTT_STATE,
    AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE,
    AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE,
    AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE,
    AT_WIFI_STATE_WAIT_TIMESTAMP,
    AT_WIFI_STATE_AP_SCAN,
    AT_WIFI_STATE_COUNT,
};


enum at_wifi_cw_states_t
{
    AT_WIFI_CW_STATE_NOT_CONN       = 0,
    AT_WIFI_CW_STATE_NO_IP          = 1,
    AT_WIFI_CW_STATE_CONNECTED      = 2,
    AT_WIFI_CW_STATE_CONNECTING     = 3,
    AT_WIFI_CW_STATE_DISCONNECTED   = 4,
};


typedef enum
{
    AT_WIFI_AP_ECN_OPEN             = 0,
    AT_WIFI_AP_ECN_WEP              = 1,
    AT_WIFI_AP_ECN_WPA_PSK          = 2,
    AT_WIFI_AP_ECN_WPA2_PSK         = 3,
    AT_WIFI_AP_ECN_WPA_WPA2_PSK     = 4,
    AT_WIFI_AP_ECN_WPA2_ENTERPRISE  = 5,
    AT_WIFI_AP_ECN_WPA3_PSK         = 6,
    AT_WIFI_AP_ECN_WPA2_WPA3_PSK    = 7,
    AT_WIFI_AP_ECN_WAPI_PSK         = 8,
    AT_WIFI_AP_ECN_OWE              = 9,
    AT_WIFI_AP_ECN_COUNT
} at_wifi_ap_ecn_t;


typedef enum
{
    AT_WIFI_AP_CIPHER_NONE          = 0,
    AT_WIFI_AP_CIPHER_WEP40         = 1,
    AT_WIFI_AP_CIPHER_WEP104        = 2,
    AT_WIFI_AP_CIPHER_TKIP          = 3,
    AT_WIFI_AP_CIPHER_CCMP          = 4,
    AT_WIFI_AP_CIPHER_TKIP_CCMP     = 5,
    AT_WIFI_AP_CIPHER_AES_CMAC_128  = 6,
    AT_WIFI_AP_CIPHER_UNKNOWN       = 7,
    AT_WIFI_AP_CIPHER_COUNT
} at_wifi_ap_cipher_t;


typedef struct
{
    at_wifi_ap_ecn_t    ecn;
    char                ssid[AT_WIFI_MAX_SSID_LEN + 1];
    int                 rssi;
    uint8_t             mac[6];
    unsigned            channel;
    int                 freq_offset;
    int                 freqcal_val;
    at_wifi_ap_cipher_t pairwise_cipher;
    at_wifi_ap_cipher_t group_cipher;
    uint8_t             bgn:3;
    uint8_t             wps:1;
    uint8_t             _:4;
} at_wifi_ap_info_t;


static struct
{
    enum at_wifi_states_t    state;
    enum at_wifi_states_t    before_timedout_state;
    char                    before_timedout_last_cmd[AT_BASE_MAX_CMD_LEN];
    at_wifi_config_t*       mem;
    uint32_t                ip;
    at_mqtt_ctx_t           mqtt_ctx;
    at_wifi_ap_info_t       ap_info_list[AT_WIFI_AP_LIST_LEN];
    unsigned                ap_info_len;
    enum at_wifi_states_t   before_ap_list_state;
    bool                    autoconnect;
} _at_wifi_ctx =
{
    .state                      = AT_WIFI_STATE_OFF,
    .before_timedout_state      = AT_WIFI_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
    .mqtt_ctx                   =
    {
        .mem                    = NULL,
        .topic_header           = "osm/unknown",
        .publish_packet         = {.message = {0}, .len = 0},
        .at_base_ctx             =
        {
            .last_sent          = 0,
            .last_recv          = 0,
            .off_since          = 0,
            .reset_pin          = COMMS_RESET_PORT_N_PINS,
            .boot_pin           = COMMS_BOOT_PORT_N_PINS,
            .time               = {.ts_unix = 0, .sys = 0},
        },
    },
    .ap_info_list = {{0}},
    .ap_info_len = 0,
    .autoconnect = true,
};


static struct cmd_link_t* _at_wifi_config_cmds;


static const char * _at_wifi_get_state_str(enum at_wifi_states_t state)
{
    static const char* state_strs[] =
    {
        "OFF"                                           ,
        "IS_CONNECTED"                                  ,
        "RESTORE"                                       ,
        "DISABLE_ECHO"                                  ,
        "WAIT_MAC_ADDRESS"                              ,
        "NO_CONF"                                       ,
        "RF_REGION"                                     ,
        "WIFI_SETTING_MODE"                             ,
        "WIFI_INIT"                                     ,
        "WIFI_CONNECTING"                               ,
        "SNTP_WAIT_SET"                                 ,
        "MQTT_WAIT_USR_CONF"                            ,
        "MQTT_WAIT_CONF"                                ,
        "MQTT_CONNECTING"                               ,
        "MQTT_WAIT_SUB"                                 ,
        "AT_WIFI_STATE_MQTT_IS_CONNECTED"               ,
        "AT_WIFI_STATE_MQTT_IS_SUBSCRIBED"              ,
        "IDLE"                                          ,
        "MQTT_WAIT_PUB"                                 ,
        "MQTT_PUBLISHING"                               ,
        "MQTT_FAIL_CONNECT"                             ,
        "WIFI_FAIL_CONNECT"                             ,
        "WAIT_WIFI_STATE"                               ,
        "WAIT_MQTT_STATE"                               ,
        "TIMEDOUT_WIFI_WAIT_STATE"                      ,
        "TIMEDOUT_MQTT_WAIT_WIFI_STATE"                 ,
        "TIMEDOUT_MQTT_WAIT_MQTT_STATE"                 ,
        "WAIT_TIMESTAMP"                                ,
        "AP_SCAN"                                       ,
    };
    _Static_assert(sizeof(state_strs)/sizeof(state_strs[0]) == AT_WIFI_STATE_COUNT, "Wrong number of states");
    unsigned _state = (unsigned)state;
    if (_state >= AT_WIFI_STATE_COUNT)
        return "<INVALID>";
    return state_strs[_state];
}




static bool _at_wifi_mem_is_valid(void)
{

    return _at_wifi_ctx.mem &&
        at_mqtt_mem_is_valid() &&
        str_is_valid_ascii(_at_wifi_ctx.mem->wifi.ssid   , AT_WIFI_MAX_SSID_LEN          , true  ) &&
        str_is_valid_ascii(_at_wifi_ctx.mem->wifi.pwd    , AT_WIFI_MAX_PWD_LEN           , false );
}


static unsigned _at_wifi_printf(const char* fmt, ...)
{
    comms_debug("Command when in state:%s", _at_wifi_get_state_str(_at_wifi_ctx.state));
    char* buf = _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_cmd.str;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, AT_BASE_MAX_CMD_LEN - 2, fmt, args);
    va_end(args);
    buf[len] = 0;
    _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_sent = get_since_boot_ms();
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = 0;
    len = at_base_raw_send(buf, len+1);
    return len;
}


static void _at_wifi_start(void)
{
    if (AT_WIFI_STATE_RESTORE == _at_wifi_ctx.state)
    {
        comms_debug("Already resetting, nop");
    }
    else
    {
        _at_wifi_printf("AT+CWSTATE?");
        _at_wifi_ctx.state = AT_WIFI_STATE_IS_CONNECTED;
    }
}

static void _at_wifi_is_mqtt_connected(void)
{
    _at_wifi_printf("AT+MQTTCONN?");
    _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_IS_CONNECTED;
}

static void _at_wifi_is_mqtt_subscribed(void)
{
    _at_wifi_printf("AT+MQTTSUB?");
    _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_IS_SUBSCRIBED;
}


static void _at_wifi_reset(void)
{
    comms_debug("RESET when in state:%s", _at_wifi_get_state_str(_at_wifi_ctx.state));
    comms_debug("AT wifi reset");
    _at_wifi_ctx.mqtt_ctx.at_base_ctx.off_since = get_since_boot_ms();
    _at_wifi_printf("AT+RESTORE");
    _at_wifi_ctx.state = AT_WIFI_STATE_RESTORE;
}


static void _at_wifi_hw_reset(void)
{
    comms_debug("AT wifi HW reset");
    at_base_ctx_t* base_ctx = &_at_wifi_ctx.mqtt_ctx.at_base_ctx;
    base_ctx->off_since = get_since_boot_ms();
    platform_gpio_set(&base_ctx->reset_pin, false);
    spin_blocking_ms(1);
    platform_gpio_set(&base_ctx->reset_pin, true);
    _at_wifi_ctx.state = AT_WIFI_STATE_RESTORE;
}


static unsigned _at_wifi_mqtt_publish(const char* topic, char* message, unsigned message_len)
{
    unsigned ret_len = 0;
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_IDLE:
        {
            message_len = message_len < COMMS_DEFAULT_MTU ? message_len : COMMS_DEFAULT_MTU;
            at_base_cmd_t* cmd = at_mqtt_publish_prep(topic, message, message_len);
            if (!cmd)
            {
                comms_debug("Failed to prep MQTT");
            }
            else
            {
                _at_wifi_printf(cmd->str);
                _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_PUB;
                ret_len = message_len;
            }
            break;
        }
        default:
            comms_debug("Wrong state to publish packet: %u", _at_wifi_ctx.state);
            break;
    }
    return ret_len;
}


uint16_t at_wifi_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool at_wifi_send_ready(void)
{
    return _at_wifi_ctx.state == AT_WIFI_STATE_IDLE;
}


bool at_wifi_send_allowed(void)
{
    return false;
}


static bool _at_wifi_mqtt_publish_measurements(char* data, uint16_t len)
{
    return _at_wifi_mqtt_publish(AT_MQTT_TOPIC_MEASUREMENTS, data, len) > 0;
}


bool at_wifi_send(char* data, uint16_t len)
{
    bool ret = _at_wifi_mqtt_publish_measurements(data, len);
    if (!ret)
    {
        comms_debug("Failed to send measurement");
    }
    return ret;
}


static command_response_t _at_wifi_config_wifi_ssid_cb(char* args, cmd_ctx_t * ctx);
static command_response_t _at_wifi_config_wifi_pwd_cb(char* args, cmd_ctx_t * ctx);
static command_response_t _at_wifi_config_country_cb(char* args, cmd_ctx_t* ctx);
static command_response_t _at_wifi_config_channel_start_cb(char* args, cmd_ctx_t* ctx);
static command_response_t _at_wifi_config_channel_count_cb(char* args, cmd_ctx_t* ctx);


void at_wifi_init(void)
{
    static struct cmd_link_t config_cmds[] =
    {
        { "wifi_ssid",      "Set/get SSID",                 _at_wifi_config_wifi_ssid_cb        , false , NULL },
        { "wifi_pwd",       "Set/get password",             _at_wifi_config_wifi_pwd_cb         , false , NULL },
        { "country",        "Set/get country",              _at_wifi_config_country_cb          , false , NULL },
        { "schan",          "Set/get start channel",        _at_wifi_config_channel_start_cb    , false , NULL },
        { "nchan",          "Set/get number of channels",   _at_wifi_config_channel_count_cb    , false , NULL },
    };

    struct cmd_link_t* tail = &config_cmds[ARRAY_SIZE(config_cmds)-1];

    for (struct cmd_link_t* cur = config_cmds; cur != tail; cur++)
        cur->next = cur + 1;

    at_mqtt_add_commands(tail);
    _at_wifi_config_cmds = config_cmds;

    _at_wifi_ctx.mem = (at_wifi_config_t*)&persist_data.model_config.comms_config;

    _at_wifi_ctx.mqtt_ctx.mem = &_at_wifi_ctx.mem->mqtt;

    at_mqtt_init(&_at_wifi_ctx.mqtt_ctx);
    _at_wifi_ctx.state = AT_WIFI_STATE_OFF;
}


void at_wifi_reset(void)
{
    _at_wifi_reset();
}


static void _at_wifi_process_state_off(char* msg, unsigned len)
{
    const char ready_msg[] = "ready";
    if (is_str(ready_msg, msg, len))
    {
        _at_wifi_start();
    }
}


static void _at_wifi_process_state_is_connected(char* msg, unsigned len)
{
    /* +CWSTATE:<state>,<"ssid">
     * 0 is not connecting, 1,2,3 is connecting or connected, 4 is
     * disconnected.
     */
    const char cwstate_msg[] = "+CWSTATE:";
    unsigned cwstate_msg_len = strlen(cwstate_msg);
    static enum at_wifi_cw_states_t _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
    if (is_str(cwstate_msg, msg, cwstate_msg_len))
    {
        char* p, * np;
        unsigned len_rem = len - cwstate_msg_len;
        p = msg + cwstate_msg_len;
        uint8_t state = strtoul(p, &np, 10);
        if (p != np && len_rem && np[0] == ',' && np[1] == '"' && msg[len-1] == '"')
        {
            switch (state)
            {
                case AT_WIFI_CW_STATE_CONNECTED:
                    /* fall through */
                case AT_WIFI_CW_STATE_NO_IP:
                    /* fall through */
                case AT_WIFI_CW_STATE_CONNECTING:
                {
                    char* ssid = np + 2;
                    unsigned ssid_len = (msg + len) > (ssid + 1) ? msg + len - ssid - 1 : 0;
                    /* Probably should check if saved has trailing spaces */
                    char* cmp_ssid = _at_wifi_ctx.mem->wifi.ssid;
                    unsigned cmp_ssid_len = strnlen(cmp_ssid, AT_WIFI_MAX_SSID_LEN);
                    if (ssid_len == cmp_ssid_len &&
                        strncmp(cmp_ssid, ssid, cmp_ssid_len) == 0)
                    {
                        _wifi_state = state;
                    }
                    break;
                }
                case AT_WIFI_CW_STATE_DISCONNECTED:
                    /* fall through */
                case AT_WIFI_CW_STATE_NOT_CONN:
                    _wifi_state = state;
                    break;
                default:
                    comms_debug("Unknown CWSTATE:%"PRIu8, state);
                    _at_wifi_reset();
                    break;
            }
        }
    }
    else if (at_base_is_ok(msg, len))
    {
        switch (_wifi_state)
        {
            case AT_WIFI_CW_STATE_CONNECTED:
                _at_wifi_is_mqtt_connected();
                break;
            case AT_WIFI_CW_STATE_NO_IP:
                /* fall through */
            case AT_WIFI_CW_STATE_CONNECTING:
                _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
                break;
            case AT_WIFI_CW_STATE_NOT_CONN:
                /* fall through */
            case AT_WIFI_CW_STATE_DISCONNECTED:
                /* fall through */
            default:
                _at_wifi_reset();
                break;
        }
        _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
    }
    else if (at_base_is_error(msg, len))
    {
        _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_restore(char* msg, unsigned len)
{
    const char ready[] = "ready";
    if (is_str(ready, msg, len))
    {
        _at_wifi_printf("ATE0");
        _at_wifi_ctx.state = AT_WIFI_STATE_DISABLE_ECHO;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_disable_echo(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        _at_wifi_printf("AT+CIPSTAMAC?");
        _at_wifi_ctx.state = AT_WIFI_STATE_WAIT_MAC_ADDRESS;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_send_rf_region(void)
{
    _at_wifi_printf(
        "AT+CWCOUNTRY=0,\"%s\",%"PRIu16",%"PRIu16,
        AT_BASE_SANIT_STR(_at_wifi_ctx.mem->country_code),
        _at_wifi_ctx.mem->channel_start,
        _at_wifi_ctx.mem->channel_count
        );
    _at_wifi_ctx.state = AT_WIFI_STATE_RF_REGION;
}


static void _at_wifi_process_state_wait_mac_address(char* msg, unsigned len)
{
    const char mac_msg[] = "+CIPSTAMAC:\"";
    unsigned mac_msg_len = strlen(mac_msg);
    if (is_str(mac_msg, msg, mac_msg_len))
    {
        unsigned len_rem = len - mac_msg_len;
        const unsigned maxstrlen = AT_BASE_MAC_ADDRESS_LEN - 1;
        len_rem = len_rem > maxstrlen ? maxstrlen : len_rem;
        char* dest = _at_wifi_ctx.mqtt_ctx.at_base_ctx.mac_address;
        memcpy(dest, msg + mac_msg_len, len_rem);
        dest[maxstrlen] = 0;
    }
    else if (at_base_is_ok(msg, len))
    {
        if (_at_wifi_mem_is_valid())
        {
            _at_wifi_send_rf_region();
        }
        else
        {
            _at_wifi_ctx.state = AT_WIFI_STATE_NO_CONF;
        }
    }
}


static void _at_wifi_process_state_rf_region(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        at_base_sleep();
        _at_wifi_printf("AT+CWMODE=1");
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_SETTING_MODE;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_wifi_setting_mode(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        _at_wifi_printf("AT+CWINIT=1");
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_INIT;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_wifi_init(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        at_base_sleep();
        char ssid[2*AT_WIFI_MAX_SSID_LEN+1];
        char* san_ssid = AT_BASE_SANIT_STR(_at_wifi_ctx.mem->wifi.ssid);
        strncpy(ssid, san_ssid, 2*AT_WIFI_MAX_SSID_LEN+1);
        _at_wifi_printf(
            "AT+CWJAP=\"%s\",\"%s\"",
            ssid,
            AT_BASE_SANIT_STR(_at_wifi_ctx.mem->wifi.pwd)
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_wifi_conn(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    if (at_base_is_ok(msg, len))
    {
        at_base_cmd_t* cmd = at_mqtt_get_ntp_cfg();
        if (!cmd)
        {
            comms_debug("Failed to get the NTP config");
        }
        else
        {
            _at_wifi_printf(cmd->str);
            _at_wifi_ctx.state = AT_WIFI_STATE_SNTP_WAIT_SET;
        }
    }
    else if (is_str(error_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_FAIL_CONNECT;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_do_mqtt_user_conf(void)
{
    at_base_cmd_t* cmd = at_mqtt_get_mqtt_user_cfg();
    if (!cmd)
    {
        comms_debug("Failed to get MQTT user config.");
    }
    else
    {
        _at_wifi_printf(cmd->str);
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_USR_CONF;
    }
}


static void _at_wifi_do_mqtt_sub(void)
{
    at_base_cmd_t* cmd = at_mqtt_get_mqtt_sub_cfg();
    if (!cmd)
    {
        comms_debug("Failed to get MQTT sub config.");
    }
    else
    {
        _at_wifi_printf(cmd->str);
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_SUB;
    }
}


static void _at_wifi_process_state_sntp(char* msg, unsigned len)
{
    const char time_updated_msg[] = "+TIME_UPDATED";
    if (is_str(time_updated_msg, msg, len))
    {
        at_base_sleep();
        _at_wifi_do_mqtt_user_conf();
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        at_base_cmd_t* cmd = at_mqtt_get_mqtt_conn_cfg();
        if (!cmd)
        {
            comms_debug("Failed to get MQTT connection config.");
        }
        else
        {
            _at_wifi_printf(cmd->str);
            _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_CONF;
        }
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_is_connected(char* msg, unsigned len)
{
    static enum at_mqtt_conn_states_t conn = AT_MQTT_CONN_STATE_NOT_INIT;
    const char mqtt_conn_msg[] = "+MQTTCONN:";
    unsigned mqtt_conn_msg_len = strlen(mqtt_conn_msg);
    if (is_str(mqtt_conn_msg, msg, mqtt_conn_msg_len))
    {
        char* conn_msg = msg + mqtt_conn_msg_len;
        unsigned conn_msg_len = len - mqtt_conn_msg_len;
        if (!at_mqtt_parse_mqtt_conn(conn_msg, conn_msg_len, &conn))
        {
            conn = AT_MQTT_CONN_STATE_NOT_INIT;
        }
    }
    else if (at_base_is_ok(msg, len))
    {
        switch (conn)
        {
            case AT_MQTT_CONN_STATE_CONN_EST:
                /* fall through */
            case AT_MQTT_CONN_STATE_CONN_EST_NO_TOPIC:
                /* fall through */
            case AT_MQTT_CONN_STATE_CONN_EST_WITH_TOPIC:
                _at_wifi_is_mqtt_subscribed();
                break;
            default:
                _at_wifi_do_mqtt_user_conf();
                break;
        }
        conn = AT_MQTT_CONN_STATE_NOT_INIT;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_is_subscribed(char* msg, unsigned len)
{
    static bool is_subbed = false;
    const char mqtt_sub_msg[] = "+MQTTSUB:";
    const unsigned mqtt_sub_msg_len = strlen(mqtt_sub_msg);
    if (is_str(mqtt_sub_msg, msg, mqtt_sub_msg_len))
    {
        char* sub_msg = msg + mqtt_sub_msg_len;
        unsigned sub_msg_len = len - mqtt_sub_msg_len;
        if (at_mqtt_topic_match(sub_msg, sub_msg_len, msg, len))
        {
            is_subbed = true;
        }
    }
    else if (at_base_is_ok(msg, len))
    {
        if (is_subbed)
        {
            is_subbed = false;
            _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
        }
        else
        {
            _at_wifi_do_mqtt_sub();
        }
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_conf(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        at_base_sleep();
        at_base_cmd_t* cmd = at_mqtt_get_mqtt_conn();
        if (!cmd)
        {
            comms_debug("Failed to get MQTT connection info.");
        }
        else
        {
            _at_wifi_printf(cmd->str);
            _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_CONNECTING;
        }
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        at_base_sleep();
        _at_wifi_do_mqtt_sub();
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_sub(char* msg, unsigned len)
{
    const char already_subscribe_str[] = "ALREADY SUBSCRIBE";
    if (at_base_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
    else if (is_str(already_subscribe_str, msg, len))
    {
        /* Already subscribed, assume everything fine */
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static bool _at_wifi_process_event(char* msg, unsigned len)
{
    char resp_payload[AT_MQTT_RESP_PAYLOAD_LEN + 1];
    int resp_payload_len = at_mqtt_process_event(msg, len, resp_payload, AT_MQTT_RESP_PAYLOAD_LEN + 1);
    return (AT_ERROR_CODE != resp_payload_len) &&
        _at_wifi_mqtt_publish(AT_MQTT_TOPIC_COMMAND_RESP, resp_payload, resp_payload_len);
}


static void _at_wifi_process_state_idle(char* msg, unsigned len)
{
}


static void _at_wifi_process_state_mqtt_wait_pub(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        comms_debug("Sending pub data %.*s", _at_wifi_ctx.mqtt_ctx.publish_packet.len,  _at_wifi_ctx.mqtt_ctx.publish_packet.message);
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_PUBLISHING;
        at_base_raw_send(
            _at_wifi_ctx.mqtt_ctx.publish_packet.message,
            _at_wifi_ctx.mqtt_ctx.publish_packet.len
            );
    }
    else if (at_base_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_publishing(char* msg, unsigned len)
{
    /* Prints '>' when wants to start writing message but, this isn't
     * followed by \r so isn't handed in to be processed yet. We can get
     * around this by just skipping it if it is there. (Theres new lines
     * in between if 'busy p...' is printed.
     */
    if (msg[0] == '>')
    {
        msg++;
        len--;
    }
    const char pub_ok[] = "+MQTTPUB:OK";
    if (is_str(pub_ok, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
        comms_debug("Successful send, propagating ACK");
        on_protocol_sent_ack(true);
    }
    else if (at_base_is_error(msg, len))
    {
        comms_debug("Failed send (ERROR), propagating NACK");
        on_protocol_sent_ack(false);
        _at_wifi_reset();
    }
}


static bool _at_wifi_parse_cw_state(char* state_msg, unsigned state_msg_len)
{
    /* returns true for retry, false for wifi reset */
    char* p = state_msg;
    char* np;
    uint8_t state = strtoul(p, &np, 10);
    if (p == np)
    {
        return false;
    }
    return state == AT_WIFI_CW_STATE_CONNECTED;
}


static bool _at_wifi_is_cw_state(char* msg, unsigned len, char** end)
{
    const char cw_state_msg[] = "+CWSTATE:";
    unsigned cw_state_msg_len = strnlen(cw_state_msg, len);
    bool ret = is_str(cw_state_msg, msg, cw_state_msg_len);
    if (end)
    {
        *end = msg + cw_state_msg_len;
    }
    return ret;
}


static void _at_wifi_retry_command(void)
{
    _at_wifi_printf("%s", _at_wifi_ctx.before_timedout_last_cmd);
    _at_wifi_ctx.state = _at_wifi_ctx.before_timedout_state;
}


static void _at_wifi_process_state_timedout_wifi_wait_state(char* msg, unsigned len)
{
    char* state_msg;
    if (_at_wifi_is_cw_state(msg, len, &state_msg))
    {
        unsigned state_msg_len = state_msg - msg;
        if (_at_wifi_parse_cw_state(state_msg, state_msg_len))
        {
            /* retry */
            _at_wifi_retry_command();
        }
        else
        {
            comms_debug("Failed CW state (wifi)");
            _at_wifi_hw_reset();
        }
    }
}


static void _at_wifi_process_state_timedout_mqtt_wait_wifi_state(char* msg, unsigned len)
{
    char* state_msg;
    if (_at_wifi_is_cw_state(msg, len, &state_msg))
    {
        unsigned state_msg_len = state_msg - msg;
        if (_at_wifi_parse_cw_state(state_msg, state_msg_len))
        {
            _at_wifi_printf("AT+MQTTCONN?");
            _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE;
        }
        else
        {
            comms_debug("Failed CW state (mqtt)");
            _at_wifi_hw_reset();
        }
    }
}


static void _at_wifi_process_state_timedout_mqtt_wait_mqtt_state(char* msg, unsigned len)
{
    const char mqtt_conn_msg[] = "+MQTTCONN:";
    unsigned mqtt_conn_msg_len = strlen(mqtt_conn_msg);
    if (is_str(mqtt_conn_msg, msg, mqtt_conn_msg_len))
    {
        char* conn_msg = msg + mqtt_conn_msg_len;
        unsigned conn_msg_len = len - mqtt_conn_msg_len;
        enum at_mqtt_conn_states_t conn;
        if (at_mqtt_parse_mqtt_conn(conn_msg, conn_msg_len, &conn) &&
            conn >= AT_MQTT_CONN_STATE_CONN_EST && conn < AT_MQTT_CONN_STATE_COUNT)
        {
            _at_wifi_retry_command();
        }
        else
        {
            comms_debug("Failed MQTT state (mqtt)");
            _at_wifi_reset();
        }
    }
}


static void _at_wifi_process_state_wait_timestamp(char* msg, unsigned len)
{
    const char sys_ts_msg[] = "+SYSTIMESTAMP:";
    unsigned sys_ts_msg_len = strlen(sys_ts_msg);
    if (is_str(sys_ts_msg, msg, sys_ts_msg_len))
    {
        _at_wifi_ctx.mqtt_ctx.at_base_ctx.time.ts_unix = strtoull(&msg[sys_ts_msg_len], NULL, 10);
        _at_wifi_ctx.mqtt_ctx.at_base_ctx.time.sys = get_since_boot_ms();
    }
    else if (at_base_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
}


static bool _at_wifi_process_stat_ap_scan_parse(char* msg, unsigned len, at_wifi_ap_info_t* ap_info)
{
    char* p = msg;
    char* np;
    if (*p != '(')
    {
        comms_debug("Bad format (start)");
        return false;
    }
    p++;
    unsigned enc_type = strtoul(p, &np, 10);
    if (p == np || *np != ',' || AT_WIFI_AP_ECN_COUNT <= enc_type || np > msg + len)
    {
        comms_debug("Bad format (encryption method)");
        return false;
    }
    ap_info->ecn = enc_type;
    p = np + 1;
    if (*p != '"')
    {
        comms_debug("Bad format (SSID start)");
        return false;
    }
    p++;
    np = strchr(p, '"');
    /* If the quotation mark is escaped, then look for next one */
    while (np && np > p && np[-1] == '\\')
    {
        np = strchr(np + 1, '"');
    }
    if (np <= p || AT_WIFI_MAX_SSID_LEN < np - p || np + 1 > msg + len)
    {
        comms_debug("Bad format (SSID)");
        return false;
    }
    if (*np != '"' && *(np + 1) != ',')
    {
        comms_debug("Bad format (SSID end)");
        return false;
    }
    unsigned ssid_len = np - p;
    memcpy(ap_info->ssid, p, ssid_len);
    ap_info->ssid[ssid_len] = 0;
    p = np + 2;
    ap_info->rssi = strtoul(p, &np, 10);
    if (p == np || *np != ',' || np > msg + len)
    {
        comms_debug("Bad format (RSSI)");
        return false;
    }
    p = np + 1;
    if (*p != '"')
    {
        comms_debug("Bad format (MAC start)");
        return false;
    }
    p++;
    for (unsigned i = 0; i < 6; i++)
    {
        ap_info->mac[i] = strtoul(p, &np, 16);
        if (np <= p ||
            ((i == 5 && *np != '"') || (i != 5 && *np != ':'))
            || np + 1 > msg + len)
        {
            comms_debug("Bad format (MAC)");
            return false;
        }
        p = np + 1;
    }
    if (*p != ',')
    {
        comms_debug("Bad format (MAC end)");
        return false;
    }
    p++;
    ap_info->channel = strtoul(p, &np, 10);
    if (p == np || *np != ',' || np > msg + len)
    {
        comms_debug("Bad format (channel)");
        return false;
    }
    p = np + 1;
    ap_info->freq_offset = strtoul(p, &np, 10);
    if (p == np || *np != ',' || np > msg + len)
    {
        comms_debug("Bad format (freq offset)");
        return false;
    }
    p = np + 1;
    ap_info->freqcal_val = strtoul(p, &np, 10);
    if (p == np || *np != ',' || np > msg + len)
    {
        comms_debug("Bad format (freq cal)");
        return false;
    }
    p = np + 1;
    unsigned pairwise_cipher = strtoul(p, &np, 10);
    if (p == np || *np != ',' || AT_WIFI_AP_CIPHER_COUNT <= pairwise_cipher || np > msg + len)
    {
        comms_debug("Bad format (pairwise cipher)");
        return false;
    }
    ap_info->pairwise_cipher = pairwise_cipher;
    p = np + 1;
    unsigned group_cipher = strtoul(p, &np, 10);
    if (p == np || *np != ',' || AT_WIFI_AP_CIPHER_COUNT <= group_cipher || np > msg + len)
    {
        comms_debug("Bad format (group cipher)");
        return false;
    }
    ap_info->group_cipher = group_cipher;
    p = np + 1;
    unsigned bgn = strtoul(p, &np, 10);
    if (p == np || *np != ',' || bgn & ~(0xFF & 0x7) || np > msg + len)
    {
        comms_debug("Bad format (bgn)");
        return false;
    }
    ap_info->bgn = bgn;
    p = np + 1;
    unsigned wps = strtoul(p, &np, 10);
    if (p == np || wps > 1 || np > msg + len)
    {
        comms_debug("Bad format (wps)");
        return false;
    }
    ap_info->wps = wps;
    if (*np != ')')
    {
        comms_debug("Bad format (end)");
        return false;
    }
    return true;
}

static void _at_wifi_process_state_ap_scan(char* msg, unsigned len)
{
    /* +CWLAP:(7,"Devtank Wifi",-81,"a6:b7:d6:12:5d:6b",6,-1,-1,4,4,7,0)
       +CWLAP:<ecn>,<ssid>,<rssi>,<mac>,<channel>,<freq_offset>,<freqcal_val>,<pairwise_cipher>,<group_cipher>,<bgn>,<wps>
    */
    const char cwlap_msg[] = "+CWLAP:";
    unsigned cwlap_msg_len = strlen(cwlap_msg);
    if (is_str(cwlap_msg, msg, cwlap_msg_len))
    {
        if (AT_WIFI_AP_LIST_LEN <= _at_wifi_ctx.ap_info_len)
        {
            comms_debug("Already filled list");
            return;
        }
        at_wifi_ap_info_t* ap_info = &_at_wifi_ctx.ap_info_list[_at_wifi_ctx.ap_info_len++];
        memset(ap_info, 0, sizeof(at_wifi_ap_info_t));
        char* p = &msg[cwlap_msg_len];
        unsigned p_len = msg + len - p;
        if (!_at_wifi_process_stat_ap_scan_parse(p, p_len, ap_info))
        {
            comms_debug("Failed to parse AP message");
            _at_wifi_ctx.ap_info_len--;
        }
    }
    else if (at_base_is_ok(msg, len))
    {
        comms_debug("Finished parsing APs");
        _at_wifi_ctx.state = _at_wifi_ctx.before_ap_list_state;
    }
}


void at_wifi_process(char* msg)
{
    unsigned len = strlen(msg);

    _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_recv = get_since_boot_ms();

    comms_debug("Message when in state:%s", _at_wifi_get_state_str(_at_wifi_ctx.state));

    if (_at_wifi_process_event(msg, len))
    {
        return;
    }

    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_OFF:
            _at_wifi_process_state_off(msg, len);
            break;
        case AT_WIFI_STATE_IS_CONNECTED:
            _at_wifi_process_state_is_connected(msg, len);
            break;
        case AT_WIFI_STATE_RESTORE:
            _at_wifi_process_state_restore(msg, len);
            break;
        case AT_WIFI_STATE_DISABLE_ECHO:
            _at_wifi_process_state_disable_echo(msg, len);
            break;
        case AT_WIFI_STATE_WAIT_MAC_ADDRESS:
            _at_wifi_process_state_wait_mac_address(msg, len);
            break;
        case AT_WIFI_STATE_NO_CONF:
            /* Shouldn't really be anything of importance here */
            break;
        case AT_WIFI_STATE_RF_REGION:
            _at_wifi_process_state_rf_region(msg, len);
            break;
        case AT_WIFI_STATE_WIFI_SETTING_MODE:
            _at_wifi_process_state_wifi_setting_mode(msg, len);
            break;
        case AT_WIFI_STATE_WIFI_INIT:
            _at_wifi_process_state_wifi_init(msg, len);
            break;
        case AT_WIFI_STATE_WIFI_CONNECTING:
            _at_wifi_process_state_wifi_conn(msg, len);
            break;
        case AT_WIFI_STATE_SNTP_WAIT_SET:
            _at_wifi_process_state_sntp(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_IS_CONNECTED:
            _at_wifi_process_state_mqtt_is_connected(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_IS_SUBSCRIBED:
            _at_wifi_process_state_mqtt_is_subscribed(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_WAIT_USR_CONF:
            _at_wifi_process_state_mqtt_wait_usr_conf(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_WAIT_CONF:
            _at_wifi_process_state_mqtt_wait_conf(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_CONNECTING:
            _at_wifi_process_state_mqtt_connecting(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_WAIT_SUB:
            _at_wifi_process_state_mqtt_wait_sub(msg, len);
            break;
        case AT_WIFI_STATE_IDLE:
            _at_wifi_process_state_idle(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_WAIT_PUB:
            _at_wifi_process_state_mqtt_wait_pub(msg, len);
            break;
        case AT_WIFI_STATE_MQTT_PUBLISHING:
            _at_wifi_process_state_mqtt_publishing(msg, len);
            break;
        case AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE:
            _at_wifi_process_state_timedout_wifi_wait_state(msg, len);
            break;
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE:
            _at_wifi_process_state_timedout_mqtt_wait_wifi_state(msg, len);
            break;
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE:
            _at_wifi_process_state_timedout_mqtt_wait_mqtt_state(msg, len);
            break;
        case AT_WIFI_STATE_WAIT_TIMESTAMP:
            _at_wifi_process_state_wait_timestamp(msg, len);
            break;
        case AT_WIFI_STATE_AP_SCAN:
            _at_wifi_process_state_ap_scan(msg, len);
            break;
        default:
            break;
    }
}


bool at_wifi_get_connected(void)
{
    return _at_wifi_ctx.state == AT_WIFI_STATE_IDLE ||
           _at_wifi_ctx.state == AT_WIFI_STATE_MQTT_WAIT_PUB ||
           _at_wifi_ctx.state == AT_WIFI_STATE_MQTT_PUBLISHING ||
           _at_wifi_ctx.state == AT_WIFI_STATE_WAIT_TIMESTAMP;
}


static void _at_wifi_timedout_start_wifi_status(void)
{
    strncpy(_at_wifi_ctx.before_timedout_last_cmd, _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_cmd.str, AT_BASE_MAX_CMD_LEN);
    _at_wifi_printf("AT+CWSTATE?");
    _at_wifi_ctx.before_timedout_state = _at_wifi_ctx.state;
}


static void _at_wifi_check_wifi_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_WIFI_TIMEOUT_MS_WIFI)
    {
        _at_wifi_timedout_start_wifi_status();
        _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE;
    }
}


static void _at_wifi_check_mqtt_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_WIFI_TIMEOUT_MS_MQTT)
    {
        if (_at_wifi_ctx.state == AT_WIFI_STATE_MQTT_PUBLISHING)
        {
            comms_debug("Failed send (TIMEOUT), propagating NACK");
            on_protocol_sent_ack(false);
        }
        _at_wifi_timedout_start_wifi_status();
        _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE;
    }
}


#define __AT_WIFI_TIMEOUT_TEMPLATE(_timeout, _cb)                      \
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.mqtt_ctx.at_base_ctx.last_sent) > _timeout) \
    {                                                                  \
        _cb();                                                         \
    }


static void _at_wifi_wifi_fail_connect(void)
{
    __AT_WIFI_TIMEOUT_TEMPLATE(AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS, _at_wifi_reset);
}


static void _at_wifi_mqtt_fail_connect(void)
{
    __AT_WIFI_TIMEOUT_TEMPLATE(AT_WIFI_MQTT_FAIL_CONNECT_TIMEOUT_MS, _at_wifi_do_mqtt_user_conf);
}


static void _at_wifi_check_default_timeout(void)
{
    __AT_WIFI_TIMEOUT_TEMPLATE(AT_WIFI_TIMEOUT_MS_DEFAULT, _at_wifi_hw_reset)
}


void at_wifi_loop_iteration(void)
{
    uint32_t now = get_since_boot_ms();

    /* Check timeout */
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_RESTORE:
            /* fall through */
        case AT_WIFI_STATE_OFF:
        {
            uint32_t delta = since_boot_delta(now, _at_wifi_ctx.mqtt_ctx.at_base_ctx.off_since);
            if (delta > AT_WIFI_STILL_OFF_TIMEOUT)
            {
                _at_wifi_hw_reset();
            }
            break;
        }
        case AT_WIFI_STATE_NO_CONF:
            /* Do nothing */
            break;
        case AT_WIFI_STATE_WIFI_INIT:
            /* fall through */
        case AT_WIFI_STATE_WIFI_SETTING_MODE:
            /* fall through */
        case AT_WIFI_STATE_WIFI_CONNECTING:
            /* fall through */
        case AT_WIFI_STATE_SNTP_WAIT_SET:
            _at_wifi_check_wifi_timeout();
            break;
        case AT_WIFI_STATE_MQTT_WAIT_USR_CONF:
            /* fall through */
        case AT_WIFI_STATE_MQTT_WAIT_CONF:
            /* fall through */
        case AT_WIFI_STATE_MQTT_CONNECTING:
            /* fall through */
        case AT_WIFI_STATE_MQTT_WAIT_SUB:
            /* fall through */
        case AT_WIFI_STATE_MQTT_WAIT_PUB:
            /* fall through */
        case AT_WIFI_STATE_MQTT_PUBLISHING:
            _at_wifi_check_mqtt_timeout();
            break;
        case AT_WIFI_STATE_WIFI_FAIL_CONNECT:
            _at_wifi_wifi_fail_connect();
            break;
        case AT_WIFI_STATE_MQTT_FAIL_CONNECT:
            _at_wifi_mqtt_fail_connect();
            break;
        case AT_WIFI_STATE_IDLE:
            /* Do nothing */
            break;
        default:
            _at_wifi_check_default_timeout();
            break;
    }
}


static void _at_wifi_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx)
{
    unsigned len = strlen(src);
    if (len)
    {
        if (len > max_dest_len)
            len = max_dest_len;
        /* Set */
        memset(dest, 0, max_dest_len+1 /* Set full space, including final null*/);
        memcpy(dest, src, len);
    }
    /* Get */
    cmd_ctx_out(ctx,"%s: %.*s", name, max_dest_len, dest);
}


static command_response_t _at_wifi_config_wifi_ssid_cb(char* args, cmd_ctx_t * ctx)
{
    _at_wifi_config_get_set_str(
        "SSID",
        _at_wifi_ctx.mem->wifi.ssid,
        AT_WIFI_MAX_SSID_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_wifi_pwd_cb(char* args, cmd_ctx_t * ctx)
{
    _at_wifi_config_get_set_str(
        "PWD",
        _at_wifi_ctx.mem->wifi.pwd,
        AT_WIFI_MAX_PWD_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_country_cb(char* args, cmd_ctx_t* ctx)
{
    _at_wifi_config_get_set_str(
        "COUNTRY",
        _at_wifi_ctx.mem->country_code,
        AT_WIFI_MAX_COUNTRY_CODE_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_channel_start_cb(char* args, cmd_ctx_t* ctx)
{
    at_base_config_get_set_u16(
        "SCHAN",
        &_at_wifi_ctx.mem->channel_start,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_channel_count_cb(char* args, cmd_ctx_t* ctx)
{
    at_base_config_get_set_u16(
        "NCHAN",
        &_at_wifi_ctx.mem->channel_count,
        args, ctx);
    return COMMAND_RESP_OK;
}


void at_wifi_config_setup_str(char * str, cmd_ctx_t * ctx)
{
    at_base_config_setup_str(_at_wifi_config_cmds, str, ctx);
}


bool at_wifi_get_id(char* str, uint8_t len)
{
    strncpy(str, _at_wifi_ctx.mqtt_ctx.topic_header, len);
    return true;
}


static command_response_t _at_wifi_send_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = skip_space(args);
    return at_base_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_wifi_cmd_config_cb(char * args, cmd_ctx_t * ctx)
{
    at_wifi_config_t before_config;
    memcpy(&before_config, _at_wifi_ctx.mem, sizeof(at_wifi_config_t));
    command_response_t ret = at_base_config_setup_str(_at_wifi_config_cmds, skip_space(args), ctx);
    if (_at_wifi_mem_is_valid())
    {
        if (AT_WIFI_STATE_NO_CONF == _at_wifi_ctx.state)
        {
            _at_wifi_send_rf_region();
        }
        else if (_at_wifi_ctx.autoconnect &&
            at_wifi_persist_config_cmp(&before_config, _at_wifi_ctx.mem))
        {
            _at_wifi_reset();
        }
    }
    return ret;
}

static bool _at_wifi_get_mac_address(char* buf, unsigned buflen);

command_response_t at_wifi_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx)
{
    char mac_address[AT_BASE_MAC_ADDRESS_LEN];
    if (!_at_wifi_get_mac_address(mac_address, AT_BASE_MAC_ADDRESS_LEN))
    {
        strncpy(mac_address, "UNKNOWN", AT_BASE_MAC_ADDRESS_LEN-1);
    }
    cmd_ctx_out(ctx,AT_BASE_PRINT_CFG_JSON_HEADER);
    cmd_ctx_flush(ctx);
    COMMS_COMMON_JSON_OUT_STR(AT_WIFI_PRINT_CFG_JSON_WIFI_SSID      , _at_wifi_ctx.mem->wifi.ssid       , AT_WIFI_MAX_SSID_LEN      );
    COMMS_COMMON_JSON_OUT_STR(AT_WIFI_PRINT_CFG_JSON_WIFI_PWD       , _at_wifi_ctx.mem->wifi.pwd        , AT_WIFI_MAX_PWD_LEN       );
    COMMS_COMMON_JSON_OUT_INT(AT_WIFI_PRINT_CFG_JSON_COUNTRY_CODE   , _at_wifi_ctx.mem->country_code                                );
    COMMS_COMMON_JSON_OUT_INT(AT_WIFI_PRINT_CFG_JSON_CHANNEL_START  , _at_wifi_ctx.mem->channel_start                               );
    COMMS_COMMON_JSON_OUT_INT(AT_WIFI_PRINT_CFG_JSON_CHANNEL_COUNT  , _at_wifi_ctx.mem->channel_count                               );
    at_mqtt_cmd_j_cfg(ctx);
    COMMS_COMMON_JSON_OUT_STR(AT_BASE_PRINT_CFG_JSON_MAC_ADDRESS    , mac_address                       , AT_BASE_MAC_ADDRESS_LEN   );
    cmd_ctx_out(ctx,AT_BASE_PRINT_CFG_JSON_TAIL);
    cmd_ctx_flush(ctx);
    return COMMAND_RESP_OK;
}


command_response_t at_wifi_cmd_conn_cb(char* args, cmd_ctx_t * ctx)
{
    if (at_wifi_get_connected())
    {
        cmd_ctx_out(ctx,"1 | Connected");
    }
    else
    {
        cmd_ctx_out(ctx,"0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_boot_cb(char* args, cmd_ctx_t * ctx)
{
    at_base_boot(args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_reset_cb(char* args, cmd_ctx_t * ctx)
{
    at_base_reset(args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_state_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"State: %s(%u)", _at_wifi_get_state_str(_at_wifi_ctx.state), (unsigned)_at_wifi_ctx.state);
    return COMMAND_RESP_OK;
}

static command_response_t _at_wifi_restart_cb(char* args, cmd_ctx_t * ctx)
{
    _at_wifi_reset();
    return COMMAND_RESP_OK;
}


static void _at_start_station_scan(void)
{
    _at_wifi_printf("AT+CWLAP");
    _at_wifi_ctx.state = AT_WIFI_STATE_AP_SCAN;
}


static bool _at_wifi_list_iteration(void* userdata)
{
    return _at_wifi_ctx.state != AT_WIFI_STATE_AP_SCAN;
}


static const char* _at_wifi_encryption_text(at_wifi_ap_ecn_t ecn)
{
    switch (ecn)
    {
        case AT_WIFI_AP_ECN_OPEN:
            return "OPEN";
        case AT_WIFI_AP_ECN_WEP:
            return "WEP";
        case AT_WIFI_AP_ECN_WPA_PSK:
            return "WPA_PSK";
        case AT_WIFI_AP_ECN_WPA2_PSK:
            return "WPA2_PSK";
        case AT_WIFI_AP_ECN_WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
        case AT_WIFI_AP_ECN_WPA2_ENTERPRISE:
            return "WPA2_ENTERPRISE";
        case AT_WIFI_AP_ECN_WPA3_PSK:
            return "WPA3_PSK";
        case AT_WIFI_AP_ECN_WPA2_WPA3_PSK:
            return "WPA2_WPA3_PSK";
        case AT_WIFI_AP_ECN_WAPI_PSK:
            return "WAPI_PSK";
        case AT_WIFI_AP_ECN_OWE:
            return "OWE";
        default:
            break;
    }
    return "Unknown";
}


static const char* _at_wifi_cipher_text(at_wifi_ap_cipher_t cipher)
{
    switch (cipher)
    {
        case AT_WIFI_AP_CIPHER_NONE:
            return "None";
        case AT_WIFI_AP_CIPHER_WEP40:
            return "WEP40";
        case AT_WIFI_AP_CIPHER_WEP104:
            return "WEP104";
        case AT_WIFI_AP_CIPHER_TKIP:
            return "TKIP";
        case AT_WIFI_AP_CIPHER_CCMP:
            return "CCMP";
        case AT_WIFI_AP_CIPHER_TKIP_CCMP:
            return "TKIP and CCMP";
        case AT_WIFI_AP_CIPHER_AES_CMAC_128:
            return "AES-CMAC-128";
        case AT_WIFI_AP_CIPHER_UNKNOWN:
            /* fall through */
        default:
            break;
    }
    return "Unknown";
}


static void _at_wifi_bgn_text(uint8_t bgn, char* buf, unsigned buflen)
{
    if (buflen)
    {
        char letters[] = "bgn";
        unsigned len = 3 < buflen ? 3 : buflen;
        char* p = buf;
        for (unsigned i = 0; i < len; i++)
        {
            if (bgn & (1 << i))
            {
                *p++ = letters[i];
            }
        }
        buf[buflen-1] = 0;
    }
}


static void _at_wifi_print_ap_list(cmd_ctx_t * ctx)
{
    uart_rings_out_drain();
    cmd_ctx_out(ctx,"[");
    for (unsigned i = 0; i < _at_wifi_ctx.ap_info_len; i++)
    {
        at_wifi_ap_info_t* ap_info = &_at_wifi_ctx.ap_info_list[i];
        char bgn[4];
        _at_wifi_bgn_text(ap_info->bgn, bgn, 4);
        cmd_ctx_out(ctx,"  {");
        cmd_ctx_out(ctx,"    \"SSID\":\"%.*s\",", AT_WIFI_MAX_SSID_LEN, ap_info->ssid);
        cmd_ctx_out(ctx,"    \"encryption\":\"%s\",", _at_wifi_encryption_text(ap_info->ecn));
        cmd_ctx_out(ctx,"    \"RSSI\":%d,", ap_info->rssi);
        cmd_ctx_out(ctx,"    \"MAC\":\"%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8":%02"PRIx8"\",", ap_info->mac[0],ap_info->mac[1],ap_info->mac[2],ap_info->mac[3],ap_info->mac[4],ap_info->mac[5]);
        cmd_ctx_out(ctx,"    \"channel\":%u,", ap_info->channel);
        cmd_ctx_out(ctx,"    \"freq_offset\":%d,", ap_info->freq_offset);
        cmd_ctx_out(ctx,"    \"freqcal_val\":%d,", ap_info->freqcal_val);
        cmd_ctx_out(ctx,"    \"pairwise_cipher\":\"%s\",", _at_wifi_cipher_text(ap_info->pairwise_cipher));
        cmd_ctx_out(ctx,"    \"group_cipher\":\"%s\",", _at_wifi_cipher_text(ap_info->group_cipher));
        cmd_ctx_out(ctx,"    \"bgn\":\"802.11%s\",", bgn);
        cmd_ctx_out(ctx,"    \"WPS\":\"%s\"", ap_info->wps ? "enabled" : "disabled");
        cmd_ctx_out(ctx,"  }%c", i != _at_wifi_ctx.ap_info_len - 1 ? ',' : ' ');
        uart_rings_out_drain();
    }
    cmd_ctx_out(ctx,"]");
}


static command_response_t _at_list_cb(char* args, cmd_ctx_t * ctx)
{
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_OFF:
            /* fall through */
        case AT_WIFI_STATE_IDLE:
            /* fall through */
        case AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE:
            /* fall through */
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE:
            /* fall through */
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE:
            /* fall through */
        case AT_WIFI_STATE_MQTT_FAIL_CONNECT:
            /* fall through */
        case AT_WIFI_STATE_NO_CONF:
            /* fall through */
        case AT_WIFI_STATE_WIFI_FAIL_CONNECT:
            break;
        default:
            comms_debug("Illegal state for listing AP: %s", _at_wifi_get_state_str(_at_wifi_ctx.state));
            cmd_ctx_out(ctx,"Busy");
            return COMMAND_RESP_ERR;
    }
    bool is_connected = at_wifi_get_connected();
    bool prev_measements_enabled = measurements_enabled;
    if (is_connected)
    {
        measurements_enabled = false;
    }
    _at_wifi_ctx.before_ap_list_state = _at_wifi_ctx.state;
    _at_wifi_ctx.ap_info_len = 0;
    _at_start_station_scan();
    command_response_t ret = COMMAND_RESP_OK;
    if (!main_loop_iterate_for(AT_WIFI_LIST_TIMEOUT, _at_wifi_list_iteration, NULL))
    {
        cmd_ctx_out(ctx,"Timed out waiting for AP list");
        _at_wifi_reset();
        ret = COMMAND_RESP_ERR;
    }
    else
    {
        _at_wifi_print_ap_list(ctx);
    }
    if (is_connected)
    {
        measurements_enabled = prev_measements_enabled;
    }
    return ret;
}


static command_response_t _at_wifi_autoconnect_cb(char* args, cmd_ctx_t * ctx)
{
    char* p = skip_space(args);
    char* np = NULL;
    unsigned en = strtoul(p, &np, 10);
    if (p != np)
    {
        _at_wifi_ctx.autoconnect = !!en;
    }
    cmd_ctx_out(ctx,"AUTOCONNECT:%u", (unsigned)_at_wifi_ctx.autoconnect);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_send"  , "Send at_wifi message"        , _at_wifi_send_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"            , _at_wifi_dbg_cb           , false , NULL },
        { "comms_boot",   "Enable/disable boot line"    , _at_wifi_boot_cb          , false , NULL },
        { "comms_reset",  "Enable/disable reset line"   , _at_wifi_reset_cb         , false , NULL },
        { "comms_state" , "Get Comms state"             , _at_wifi_state_cb         , false , NULL },
        { "comms_restart", "Comms restart"              , _at_wifi_restart_cb       , false , NULL },
        { "comms_list",   "List stations"               , _at_list_cb               , false , NULL },
        { "comms_autoconnect", "Enable/disable autoconnect", _at_wifi_autoconnect_cb , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


/* Return true  if different
 *        false if same      */
bool at_wifi_persist_config_cmp(void* d0, void* d1)
{
    return !(
        d0 && d1 &&
        memcmp(d0, d1, sizeof(at_wifi_config_t)) == 0);
}


static void _at_wifi_config_init2(at_wifi_config_t* at_wifi_config)
{
    memset(at_wifi_config, 0, sizeof(at_wifi_config_t));
    at_wifi_config->type = COMMS_TYPE_WIFI;
    at_wifi_config->mqtt.scheme = AT_MQTT_SCHEME_BARE;
    at_wifi_config->mqtt.port = 1883;
    strncpy(at_wifi_config->country_code, "GB", AT_WIFI_MAX_COUNTRY_CODE_LEN + 1);
    at_wifi_config->country_code[AT_WIFI_MAX_COUNTRY_CODE_LEN] = 0;
    at_wifi_config->channel_start = 1;
    at_wifi_config->channel_count = 13;
}


void at_wifi_config_init(comms_config_t* comms_config)
{
    _at_wifi_config_init2((at_wifi_config_t*)comms_config);
}


/*                  TIMESTAMP RETRIEVAL                  */
#define AT_WIFI_TS_TIMEOUT                      50


static bool _at_wifi_ts_loop_iteration(void* userdata)
{
    return _at_wifi_ctx.state != AT_WIFI_STATE_WAIT_TIMESTAMP;
}


bool at_wifi_get_unix_time(int64_t * ts)
{
    if (!ts || _at_wifi_ctx.state != AT_WIFI_STATE_IDLE)
    {
        return false;
    }
    _at_wifi_printf("AT+SYSTIMESTAMP?");
    _at_wifi_ctx.state = AT_WIFI_STATE_WAIT_TIMESTAMP;
    if (!main_loop_iterate_for(AT_WIFI_TS_TIMEOUT, _at_wifi_ts_loop_iteration, NULL))
    {
        /* Timed out - Not sure what to do with the chip really... */
        comms_debug("Timed out");
        return false;
    }
    at_base_time_t* time = &_at_wifi_ctx.mqtt_ctx.at_base_ctx.time;
    if (!time->sys ||
        since_boot_delta(get_since_boot_ms(), time->sys) > AT_WIFI_TS_TIMEOUT)
    {
        comms_debug("Old timestamp; invalid");
        time->sys = 0;
        time->ts_unix = 0;
        return false;
    }
    *ts = (int64_t)time->ts_unix;
    time->sys = 0;
    time->ts_unix = 0;
    return true;
}


static bool _at_wifi_get_mac_address(char* buf, unsigned buflen)
{
    char* src = _at_wifi_ctx.mqtt_ctx.at_base_ctx.mac_address;
    const unsigned mac_len = AT_BASE_MAC_ADDRESS_LEN-1;
    if (strnlen(src, mac_len) != mac_len ||
        !str_is_valid_ascii(src, mac_len, true))
    {
        comms_debug("Don't have MAC address");
        return false;
    }
    if (buflen > AT_BASE_MAC_ADDRESS_LEN)
    {
        buflen = AT_BASE_MAC_ADDRESS_LEN;
    }
    memcpy(buf, src, buflen-1);
    buf[buflen-1] = 0;
    return true;
}
