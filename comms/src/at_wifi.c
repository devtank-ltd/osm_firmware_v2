#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "at_wifi.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "common.h"
#include "log.h"
#include "cmd.h"
#include "protocol.h"

#ifndef DESIG_UNIQUE_ID0
#define DESIG_UNIQUE_ID0                        0x00C0FFEE
#endif // DESIG_UNIQUE_ID0


#define COMMS_DEFAULT_MTU                       (1024 + 128)
#define COMMS_ID_STR                            "WIFI"

#define AT_WIFI_MAX_CMD_LEN                     (1024 + 128)

#define AT_WIFI_MQTT_TOPIC_FMT                  "osm/%.*s/measurements"

#define AT_WIFI_SNTP_SERVER1                    "0.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER2                    "1.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER3                    "2.pool.ntp.org"  /* TODO */
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_CERT_KEY_ID                0
#define AT_WIFI_MQTT_CA_ID                      0
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_KEEP_ALIVE                 120 /* seconds */
#define AT_WIFI_MQTT_CLEAN_SESSION              0   /* enabled */
#define AT_WIFI_MQTT_LWT_MSG                    "{\\\"connection\\\": \\\"lost\\\"}"
#define AT_WIFI_MQTT_LWT_QOS                    0
#define AT_WIFI_MQTT_LWT_RETAIN                 0
#define AT_WIFI_MQTT_RECONNECT                  0
#define AT_WIFI_MQTT_QOS                        0
#define AT_WIFI_MQTT_RETAIN                     0

enum at_wifi_mqtt_scheme_t
{
    AT_WIFI_MQTT_SCHEME_BARE               = 1,  /* MQTT over TCP. */
    AT_WIFI_MQTT_SCHEME_TLS_NO_CERT        = 2,  /* MQTT over TLS (no certificate verify). */
    AT_WIFI_MQTT_SCHEME_TLS_VERIFY_SERVER  = 3,  /* MQTT over TLS (verify server certificate) */
};

#define AT_WIFI_MQTT_TOPIC_LEN                  63
#define AT_WIFI_MQTT_TOPIC_MEASUREMENTS         "measurements"
#define AT_WIFI_MQTT_TOPIC_COMMAND              "cmd"
#define AT_WIFI_MQTT_TOPIC_CONNECTION           "conn"
#define AT_WIFI_MQTT_TOPIC_COMMAND_RESP         AT_WIFI_MQTT_TOPIC_COMMAND"/resp"

#define AT_WIFI_MQTT_RESP_PAYLOAD_LEN           63

#define AT_WIFI_TIMEOUT_MS_WIFI                 30000
#define AT_WIFI_TIMEOUT_MS_MQTT                 30000

#define AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS    60000
#define AT_WIFI_MQTT_FAIL_CONNECT_TIMEOUT_MS    60000

#define AT_WIFI_STILL_OFF_TIMEOUT          10000


#define AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_SSID                "    \"WIFI SSID\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_PWD                 "    \"WIFI PWD\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_ADDR                "    \"MQTT ADDR\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_USER                "    \"MQTT USER\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_PWD                 "    \"MQTT PWD\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_CA                  "    \"MQTT CA\": \"%.*s\","
#define AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_PORT                "    \"MQTT PORT\": %"PRIu16""

#define AT_WIFI_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"AT WIFI\",\n\r  \"config\": {"
#define AT_WIFI_PRINT_CFG_JSON_WIFI_SSID(_wifi_ssid)        AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_SSID    , AT_WIFI_MAX_SSID_LEN      ,_wifi_ssid
#define AT_WIFI_PRINT_CFG_JSON_WIFI_PWD(_wifi_pwd)          AT_WIFI_PRINT_CFG_JSON_FMT_WIFI_PWD     , AT_WIFI_MAX_PWD_LEN       , _wifi_pwd
#define AT_WIFI_PRINT_CFG_JSON_MQTT_ADDR(_mqtt_addr)        AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_ADDR    , AT_WIFI_MQTT_ADDR_MAX_LEN , _mqtt_addr
#define AT_WIFI_PRINT_CFG_JSON_MQTT_USER(_mqtt_user)        AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_USER    , AT_WIFI_MQTT_USER_MAX_LEN , _mqtt_user
#define AT_WIFI_PRINT_CFG_JSON_MQTT_PWD(_mqtt_pwd)          AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_PWD     , AT_WIFI_MQTT_PWD_MAX_LEN  , _mqtt_pwd
#define AT_WIFI_PRINT_CFG_JSON_MQTT_CA(_mqtt_ca)            AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_CA      , AT_WIFI_MQTT_CA_MAX_LEN   , _mqtt_ca
#define AT_WIFI_PRINT_CFG_JSON_MQTT_PORT(_mqtt_port)        AT_WIFI_PRINT_CFG_JSON_FMT_MQTT_PORT    , _mqtt_port
#define AT_WIFI_PRINT_CFG_JSON_TAIL                         "  }\n\r}"


enum at_wifi_states_t
{
    AT_WIFI_STATE_OFF,
    AT_WIFI_STATE_IS_CONNECTED,
    AT_WIFI_STATE_RESTORE,
    AT_WIFI_STATE_DISABLE_ECHO,
    AT_WIFI_STATE_WIFI_INIT,
    AT_WIFI_STATE_WIFI_SETTING_MODE,
    AT_WIFI_STATE_WIFI_CONNECTING,
    AT_WIFI_STATE_SNTP_WAIT_SET,
    AT_WIFI_STATE_MQTT_WAIT_USR_CONF,
    AT_WIFI_STATE_MQTT_WAIT_CONF,
    AT_WIFI_STATE_MQTT_CONNECTING,
    AT_WIFI_STATE_MQTT_WAIT_SUB,
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


enum at_wifi_mqttconn_states_t
{
    AT_WIFI_MQTTCONN_STATE_NOT_INIT             = 0,
    AT_WIFI_MQTTCONN_STATE_SET_USR_CFG          = 1,
    AT_WIFI_MQTTCONN_STATE_SET_CONN_CFG         = 2,
    AT_WIFI_MQTTCONN_STATE_DISCONNECTED         = 3,
    AT_WIFI_MQTTCONN_STATE_CONN_EST             = 4,
    AT_WIFI_MQTTCONN_STATE_CONN_EST_NO_TOPIC    = 5,
    AT_WIFI_MQTTCONN_STATE_CONN_EST_WITH_TOPIC  = 6,
};


static struct
{
    enum at_wifi_states_t   state;
    uint32_t                last_sent;
    uint32_t                last_recv;
    uint32_t                off_since;
    char                    topic_header[AT_WIFI_MQTT_TOPIC_LEN + 1];
    char                    last_cmd[AT_WIFI_MAX_CMD_LEN];
    enum at_wifi_states_t   before_timedout_state;
    char                    before_timedout_last_cmd[AT_WIFI_MAX_CMD_LEN];
    at_wifi_config_t*       mem;
    struct
    {
        char message[COMMS_DEFAULT_MTU];
        unsigned len;
    } publish_packet;
    struct
    {
        uint64_t ts_unix;
        uint32_t sys;
    } time;
} _at_wifi_ctx =
{
    .state                      = AT_WIFI_STATE_OFF,
    .last_sent                  = 0,
    .last_recv                  = 0,
    .off_since                  = 0,
    .topic_header               = "osm/unknown",
    .last_cmd                   = {0},
    .before_timedout_state      = AT_WIFI_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
    .publish_packet             = {.message = {0}, .len = 0},
    .time                       = {.ts_unix = 0, .sys = 0},
};


static const char * _at_wifi_get_state_str(enum at_wifi_states_t state)
{
    static const char* state_strs[] =
    {
        "OFF"                                           ,
        "IS_CONNECTED"                                  ,
        "RESTORE"                                       ,
        "DISABLE_ECHO"                                  ,
        "WIFI_INIT"                                     ,
        "WIFI_SETTING_MODE"                             ,
        "WIFI_CONNECTING"                               ,
        "SNTP_WAIT_SET"                                 ,
        "MQTT_WAIT_USR_CONF"                            ,
        "MQTT_WAIT_CONF"                                ,
        "MQTT_CONNECTING"                               ,
        "MQTT_WAIT_SUB"                                 ,
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
    };
    _Static_assert(sizeof(state_strs)/sizeof(state_strs[0]) == AT_WIFI_STATE_COUNT, "Wrong number of states");
    unsigned _state = (unsigned)state;
    if (_state >= AT_WIFI_STATE_COUNT)
        return "<INVALID>";
    return state_strs[_state];
}


static bool _at_wifi_str_is_valid_ascii(char* str, unsigned max_len, bool required)
{
    unsigned len = strnlen(str, max_len);
    if (required && !len)
    {
        return false;
    }
    for (unsigned i = 0; i < len; i++)
    {
        if (!isascii(str[i]))
        {
            return false;
        }
    }
    return true;
}


static bool _at_wifi_mem_is_valid(void)
{
    at_wifi_config_t* mem = _at_wifi_ctx.mem;
    if (!mem)
    {
        return false;
    }
    return (_at_wifi_str_is_valid_ascii(mem->wifi.ssid      , AT_WIFI_MAX_SSID_LEN          , true  ) &&
            _at_wifi_str_is_valid_ascii(mem->wifi.pwd       , AT_WIFI_MAX_PWD_LEN           , false ) &&
            _at_wifi_str_is_valid_ascii(mem->mqtt.addr      , AT_WIFI_MQTT_ADDR_MAX_LEN     , true  ) &&
            _at_wifi_str_is_valid_ascii(mem->mqtt.user      , AT_WIFI_MQTT_USER_MAX_LEN     , true  ) &&
            _at_wifi_str_is_valid_ascii(mem->mqtt.pwd       , AT_WIFI_MQTT_PWD_MAX_LEN      , true  ) &&
            _at_wifi_str_is_valid_ascii(mem->mqtt.ca        , AT_WIFI_MQTT_CA_MAX_LEN       , true  ) );
}


static unsigned _at_wifi_raw_send(char* msg, unsigned len)
{
    return uart_ring_out(COMMS_UART, msg, len);
}


static unsigned _at_wifi_printf(const char* fmt, ...)
{
    comms_debug("Command when in state:%s", _at_wifi_get_state_str(_at_wifi_ctx.state));
    char* buf = _at_wifi_ctx.last_cmd;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, AT_WIFI_MAX_CMD_LEN - 2, fmt, args);
    va_end(args);
    buf[len] = 0;
    _at_wifi_ctx.last_sent = get_since_boot_ms();
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = 0;
    len = _at_wifi_raw_send(buf, len+1);
    return len;
}


static void _at_wifi_start(void)
{
    if (!_at_wifi_mem_is_valid())
    {
        comms_debug("Invalid memory");
    }
    else
    {
        /* Mem is valid */
        _at_wifi_printf("AT+CWSTATE?");
        _at_wifi_ctx.state = AT_WIFI_STATE_IS_CONNECTED;
    }
}


static void _at_wifi_reset(void)
{
    comms_debug("AT wifi reset");
    _at_wifi_ctx.off_since = get_since_boot_ms();
    _at_wifi_printf("AT+RESTORE");
    _at_wifi_ctx.state = AT_WIFI_STATE_RESTORE;
}


static unsigned _at_wifi_mqtt_publish(const char* topic, char* message, unsigned message_len)
{
    unsigned ret_len = 0;
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_IDLE:
        {
            char full_topic[AT_WIFI_MQTT_TOPIC_LEN + 1];
            unsigned full_topic_len = snprintf(
                full_topic,
                AT_WIFI_MQTT_TOPIC_LEN,
                "%.*s/%s",
                AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
                topic
                );
            full_topic[full_topic_len] = 0;
            message_len = message_len < COMMS_DEFAULT_MTU ? message_len : COMMS_DEFAULT_MTU;
            strncpy(_at_wifi_ctx.publish_packet.message, message, message_len);
            _at_wifi_ctx.publish_packet.len = message_len;
            _at_wifi_printf(
                "AT+MQTTPUBRAW=%u,\"%.*s\",%u,%u,%u",
                AT_WIFI_MQTT_LINK_ID,
                full_topic_len, full_topic,
                message_len,
                AT_WIFI_MQTT_QOS,
                AT_WIFI_MQTT_RETAIN
                );
            _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_PUB;
            ret_len = message_len;
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


bool at_wifi_send_str(char* str)
{
    if(!_at_wifi_raw_send(str, strlen(str)))
        return false;
    return _at_wifi_raw_send("\r\n", 2);
}


bool at_wifi_send_allowed(void)
{
    return false;
}


static bool _at_wifi_mqtt_publish_measurements(char* data, uint16_t len)
{
    return _at_wifi_mqtt_publish(AT_WIFI_MQTT_TOPIC_MEASUREMENTS, data, len) > 0;
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


void at_wifi_init(void)
{
    _at_wifi_ctx.mem = (at_wifi_config_t*)&persist_data.model_config.comms_config;
    unsigned topic_header_len = snprintf(
        _at_wifi_ctx.topic_header,
        AT_WIFI_MQTT_TOPIC_LEN,
        "osm/%08"PRIX32,
        DESIG_UNIQUE_ID0
        );
    _at_wifi_ctx.topic_header[topic_header_len] = 0;
    comms_debug("MQTT Topic : %s", _at_wifi_ctx.topic_header);
    _at_wifi_ctx.state = AT_WIFI_STATE_OFF;
}


void at_wifi_reset(void)
{
    _at_wifi_reset();
}


static bool _at_wifi_is_str(const char* ref, char* cmp, unsigned cmplen)
{
    const unsigned reflen = strlen(ref);
    return (reflen == cmplen && strncmp(ref, cmp, reflen) == 0);
}


static bool _at_wifi_is_ok(char* msg, unsigned len)
{
    const char ok_msg[] = "OK";
    return _at_wifi_is_str(ok_msg, msg, len);
}


static bool _at_wifi_is_error(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    return _at_wifi_is_str(error_msg, msg, len);
}


static void _at_wifi_process_state_off(char* msg, unsigned len)
{
    const char ready_msg[] = "ready";
    if (_at_wifi_mem_is_valid() && _at_wifi_is_str(ready_msg, msg, len))
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
    static bool can_skip = false;
    static enum at_wifi_cw_states_t _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
    if (_at_wifi_is_str(cwstate_msg, msg, cwstate_msg_len))
    {
        char* p, * np;
        unsigned len_rem = len - cwstate_msg_len;
        p = msg + cwstate_msg_len;
        uint8_t state = strtoul(p, &np, 10);
        if (p != np && len_rem && np[0] == ',' && np[1] == '"' && msg[len-1] == '"')
        {
            _wifi_state = state;
            if (state == AT_WIFI_CW_STATE_NO_IP ||
                state == AT_WIFI_CW_STATE_CONNECTED ||
                state == AT_WIFI_CW_STATE_CONNECTING)
            {
                char* ssid = np + 2;
                unsigned ssid_len = (msg + len) > (ssid + 1) ? msg + len - ssid - 1 : 0;
                /* Probably should check if saved has trailing spaces */
                char* cmp_ssid = _at_wifi_ctx.mem->wifi.ssid;
                unsigned cmp_ssid_len = strnlen(cmp_ssid, AT_WIFI_MAX_SSID_LEN);
                can_skip = ssid_len == cmp_ssid_len &&
                    strncmp(cmp_ssid, ssid, cmp_ssid_len) == 0;
            }
        }
    }
    else if (_at_wifi_is_ok(msg, len))
    {
        if (can_skip)
        {
            switch (_wifi_state)
            {
                case AT_WIFI_CW_STATE_CONNECTED:
                    _at_wifi_printf(
                        "AT+CIPSNTPCFG=1,0,\"%s\",\"%s\",\"%s\"",
                        AT_WIFI_SNTP_SERVER1,
                        AT_WIFI_SNTP_SERVER2,
                        AT_WIFI_SNTP_SERVER3
                        );
                    _at_wifi_ctx.state = AT_WIFI_STATE_SNTP_WAIT_SET;
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
        }
        else
        {
            _at_wifi_reset();
        }
        can_skip = false;
        _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        can_skip = false;
        _wifi_state = AT_WIFI_CW_STATE_NOT_CONN;
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_restore(char* msg, unsigned len)
{
    const char ready[] = "ready";
    if (_at_wifi_is_str(ready, msg, len))
    {
        _at_wifi_printf("ATE0");
        _at_wifi_ctx.state = AT_WIFI_STATE_DISABLE_ECHO;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_disable_echo(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_printf("AT+CWINIT=1");
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_INIT;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static bool _at_wifi_sleep_loop_iterate(void *userdata)
{
    return false;
}


static void _at_wifi_sleep(void)
{
    main_loop_iterate_for(10, _at_wifi_sleep_loop_iterate, NULL);
}


static void _at_wifi_process_state_wifi_init(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_printf("AT+CWMODE=1");
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_SETTING_MODE;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void at_wifi_process_state_wifi_setting_mode(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_printf(
            "AT+CWJAP=\"%.*s\",\"%.*s\"",
            AT_WIFI_MAX_SSID_LEN,   _at_wifi_ctx.mem->wifi.ssid,
            AT_WIFI_MAX_PWD_LEN,    _at_wifi_ctx.mem->wifi.pwd
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_wifi_conn(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_printf(
            "AT+CIPSNTPCFG=1,0,\"%s\",\"%s\",\"%s\"",
            AT_WIFI_SNTP_SERVER1,
            AT_WIFI_SNTP_SERVER2,
            AT_WIFI_SNTP_SERVER3
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_SNTP_WAIT_SET;
    }
    else if (_at_wifi_is_str(error_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_FAIL_CONNECT;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_do_mqtt_user_conf(void)
{
    enum at_wifi_mqtt_scheme_t mqtt_scheme = AT_WIFI_MQTT_SCHEME_BARE;

    const char * ca = _at_wifi_ctx.mem->mqtt.ca;

    if (ca[0] && strcmp(ca,"--") /*Ignore -- as it's just a way of not wiping*/)
    {
        comms_debug("Using MQTT over SSL.");
        mqtt_scheme = AT_WIFI_MQTT_SCHEME_TLS_NO_CERT;
    }

    _at_wifi_printf(
        "AT+MQTTUSERCFG=%u,%u,\"osm-0x%X\",\"%.*s\",\"%.*s\",%u,%u,\"\"",
        AT_WIFI_MQTT_LINK_ID,
        (unsigned)mqtt_scheme,
        DESIG_UNIQUE_ID0,
        AT_WIFI_MQTT_USER_MAX_LEN,      _at_wifi_ctx.mem->mqtt.user,
        AT_WIFI_MQTT_PWD_MAX_LEN,       _at_wifi_ctx.mem->mqtt.pwd,
        AT_WIFI_MQTT_CERT_KEY_ID,
        AT_WIFI_MQTT_CA_ID
        );
    _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_USR_CONF;
}


static void _at_wifi_process_state_sntp(char* msg, unsigned len)
{
    const char time_updated_msg[] = "+TIME_UPDATED";
    if (_at_wifi_is_str(time_updated_msg, msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_do_mqtt_user_conf();
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_printf(
            "AT+MQTTCONNCFG=%u,%u,%u,\"%.*s/%s\",\"%s\",%u,%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_KEEP_ALIVE,
            AT_WIFI_MQTT_CLEAN_SESSION,
            AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
            AT_WIFI_MQTT_TOPIC_CONNECTION,
            AT_WIFI_MQTT_LWT_MSG,
            AT_WIFI_MQTT_LWT_QOS,
            AT_WIFI_MQTT_LWT_RETAIN
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_CONF;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_conf(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_printf(
            "AT+MQTTCONN=%u,\"%.*s\",%"PRIu16",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_ADDR_MAX_LEN, _at_wifi_ctx.mem->mqtt.addr,
            _at_wifi_ctx.mem->mqtt.port,
            AT_WIFI_MQTT_RECONNECT
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_CONNECTING;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_sleep();
        _at_wifi_printf(
            "AT+MQTTSUB=%u,\"%.*s/"AT_WIFI_MQTT_TOPIC_COMMAND"\",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
            AT_WIFI_MQTT_QOS
            );
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_SUB;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_process_state_mqtt_wait_sub(char* msg, unsigned len)
{
    const char already_subscribe_str[] = "ALREADY SUBSCRIBE";
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
    else if (_at_wifi_is_str(already_subscribe_str, msg, len))
    {
        /* Already subscribed, assume everything fine */
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_do_command(char* payload, unsigned payload_len)
{
    command_response_t ret_code = cmds_process(payload, payload_len);
    char resp_payload[AT_WIFI_MQTT_RESP_PAYLOAD_LEN + 1];
    unsigned resp_payload_len = snprintf(
        resp_payload,
        AT_WIFI_MQTT_RESP_PAYLOAD_LEN,
        "{\"ret_code\":0x%"PRIX8"}",
        (uint8_t)ret_code
        );
    resp_payload[resp_payload_len] = 0;
    if (!_at_wifi_mqtt_publish(AT_WIFI_MQTT_TOPIC_COMMAND_RESP, resp_payload, resp_payload_len))
    {
        comms_debug("Failed to publish response packet");
    }
}


static bool _at_wifi_parse_payload(char* topic, unsigned topic_len, char* payload, unsigned payload_len)
{
    /* Check for this sensor */
    unsigned own_topic_header_len = strnlen(_at_wifi_ctx.topic_header, AT_WIFI_MQTT_TOPIC_LEN);
    if (topic_len >= own_topic_header_len &&
        strncmp(_at_wifi_ctx.topic_header, topic, own_topic_header_len) == 0)
    {
        char* topic_tail = topic + own_topic_header_len;
        unsigned topic_tail_len = topic_len - own_topic_header_len;
        if (topic_tail_len)
        {
            if (topic_tail[0] != '/')
            {
                /* Bad format */
                return false;
            }
            topic_tail++;
            topic_tail_len--;
        }
        if (_at_wifi_is_str(AT_WIFI_MQTT_TOPIC_COMMAND, topic_tail, topic_tail_len))
        {
            /* Topic is command */
            _at_wifi_do_command(payload, payload_len);
        }
    }
    /* leave room for potential broadcast messages */
    log_out("Command from OTA");
    return true;
}


static bool _at_wifi_parse_msg(char* msg, unsigned len)
{
    /* Destructive
     *
     * <LinkID>,<"topic">,<data_length>,data
     * */
    char* p = msg;
    char* np;
    uint8_t link_id = strtoul(p, &np, 10);
    (void)link_id;
    if (p == np)
    {
        return false;
    }
    if (np[0] != ',' || np[1] != '"')
    {
        return false;
    }
    p = np;
    char* topic = np + 2;
    np = strchr(topic, '"');
    if (!np)
    {
        return false;
    }
    unsigned topic_len = np - p - 2;
    if (topic_len > len)
    {
        return false;
    }
    topic[topic_len] = 0;
    np = topic + topic_len + 1;
    if (np[0] != ',')
    {
        return false;
    }
    p = np + 1;
    unsigned payload_length = strtoul(p, &np, 10);
    if (p == np)
    {
        return false;
    }
    if (np[0] != ',')
    {
        return false;
    }
    char* payload = np + 1;
    return _at_wifi_parse_payload(topic, topic_len, payload, payload_length);
}


static bool _at_wifi_process_event(char* msg, unsigned len)
{
    bool r = false;
    const char recv_msg[] = "+MQTTSUBRECV:";
    const unsigned recv_msg_len = strlen(recv_msg);
    if (recv_msg_len <= len &&
        _at_wifi_is_str(recv_msg, msg, recv_msg_len))
    {
        /* Received message */
        char* msg_tail = msg + recv_msg_len;
        unsigned msg_tail_len = len - recv_msg_len;
        r = _at_wifi_parse_msg(msg_tail, msg_tail_len);
    }
    return r;
}


static void _at_wifi_process_state_idle(char* msg, unsigned len)
{
}


static void _at_wifi_process_state_mqtt_wait_pub(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_PUBLISHING;
        _at_wifi_raw_send(
            _at_wifi_ctx.publish_packet.message,
            _at_wifi_ctx.publish_packet.len
            );
    }
    else if (_at_wifi_is_error(msg, len))
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
    if (_at_wifi_is_str(pub_ok, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
        comms_debug("Successful send, propagating ACK");
        on_protocol_sent_ack(true);
    }
    else if (_at_wifi_is_error(msg, len))
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
    bool ret = _at_wifi_is_str(cw_state_msg, msg, cw_state_msg_len);
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
            _at_wifi_reset();
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
            _at_wifi_reset();
        }
    }
}


static bool _at_wifi_parse_mqtt_conn(char* msg, unsigned len)
{
    char* p = msg;
    char* np;
    uint8_t link_id = strtoul(p, &np, 10);
    (void)link_id;
    if (p == np)
    {
        return false;
    }
    if (np[0] != ',')
    {
        return false;
    }
    p = np + 1;
    uint8_t conn = strtoul(p, &np, 10);
    if (p == np)
    {
        return false;
    }
    return conn == AT_WIFI_MQTTCONN_STATE_CONN_EST_WITH_TOPIC;
}


static void _at_wifi_process_state_timedout_mqtt_wait_mqtt_state(char* msg, unsigned len)
{
    const char mqtt_conn_msg[] = "+MQTTCONN:";
    unsigned mqtt_conn_msg_len = strlen(mqtt_conn_msg);
    if (_at_wifi_is_str(mqtt_conn_msg, msg, mqtt_conn_msg_len))
    {
        char* conn_msg = msg + mqtt_conn_msg_len;
        unsigned conn_msg_len = len - mqtt_conn_msg_len;
        if (_at_wifi_parse_mqtt_conn(conn_msg, conn_msg_len))
        {
            _at_wifi_retry_command();
        }
        else
        {
            _at_wifi_reset();
        }
    }
}


static void _at_wifi_process_state_wait_timestamp(char* msg, unsigned len)
{
    const char sys_ts_msg[] = "+SYSTIMESTAMP:";
    unsigned sys_ts_msg_len = strlen(sys_ts_msg);
    if (_at_wifi_is_str(sys_ts_msg, msg, sys_ts_msg_len))
    {
        _at_wifi_ctx.time.ts_unix = strtoull(&msg[sys_ts_msg_len], NULL, 10);
        _at_wifi_ctx.time.sys = get_since_boot_ms();
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
    }
}


void at_wifi_process(char* msg)
{
    unsigned len = strlen(msg);

    _at_wifi_ctx.last_recv = get_since_boot_ms();

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
        case AT_WIFI_STATE_WIFI_INIT:
            _at_wifi_process_state_wifi_init(msg, len);
            break;
        case AT_WIFI_STATE_WIFI_SETTING_MODE:
            at_wifi_process_state_wifi_setting_mode(msg, len);
            break;
        case AT_WIFI_STATE_WIFI_CONNECTING:
            _at_wifi_process_state_wifi_conn(msg, len);
            break;
        case AT_WIFI_STATE_SNTP_WAIT_SET:
            _at_wifi_process_state_sntp(msg, len);
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
        default:
            break;
    }
}


bool at_wifi_get_connected(void)
{
    return _at_wifi_ctx.state == AT_WIFI_STATE_IDLE ||
           _at_wifi_ctx.state == AT_WIFI_STATE_MQTT_WAIT_PUB ||
           _at_wifi_ctx.state == AT_WIFI_STATE_WAIT_TIMESTAMP;
}


static void _at_wifi_timedout_start_wifi_status(void)
{
    strncpy(_at_wifi_ctx.before_timedout_last_cmd, _at_wifi_ctx.last_cmd, AT_WIFI_MAX_CMD_LEN);
    _at_wifi_printf("AT+CWSTATE?");
    _at_wifi_ctx.before_timedout_state = _at_wifi_ctx.state;
}


static void _at_wifi_check_wifi_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.last_sent) > AT_WIFI_TIMEOUT_MS_WIFI)
    {
        _at_wifi_timedout_start_wifi_status();
        _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE;
    }
}


static void _at_wifi_check_mqtt_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.last_sent) > AT_WIFI_TIMEOUT_MS_MQTT)
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


static void _at_wifi_wifi_fail_connect(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.last_sent) > AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS)
    {
        _at_wifi_reset();
    }
}


static void _at_wifi_mqtt_fail_connect(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.last_sent) > AT_WIFI_MQTT_FAIL_CONNECT_TIMEOUT_MS)
    {
        /* restart MQTT config */
        _at_wifi_do_mqtt_user_conf();
    }
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
            uint32_t delta = since_boot_delta(now, _at_wifi_ctx.off_since);
            if (delta > AT_WIFI_STILL_OFF_TIMEOUT)
            {
                _at_wifi_reset();
            }
            break;
        }
        case AT_WIFI_STATE_IS_CONNECTED:
            /* fall through */
        case AT_WIFI_STATE_DISABLE_ECHO:
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
        default:
            break;
    }
}


static void _at_wifi_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src)
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
    log_out("%s: %.*s", name, max_dest_len, dest);
}


static command_response_t _at_wifi_config_wifi_ssid_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "SSID",
        _at_wifi_ctx.mem->wifi.ssid,
        AT_WIFI_MAX_SSID_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_wifi_pwd_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "PWD",
        _at_wifi_ctx.mem->wifi.pwd,
        AT_WIFI_MAX_PWD_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_mqtt_addr_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "ADDR",
        _at_wifi_ctx.mem->mqtt.addr,
        AT_WIFI_MQTT_ADDR_MAX_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_mqtt_user_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "USER",
        _at_wifi_ctx.mem->mqtt.user,
        AT_WIFI_MQTT_USER_MAX_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_mqtt_pwd_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "PWD",
        _at_wifi_ctx.mem->mqtt.pwd,
        AT_WIFI_MQTT_PWD_MAX_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_mqtt_ca_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "CA",
        _at_wifi_ctx.mem->mqtt.ca,
        AT_WIFI_MQTT_CA_MAX_LEN,
        args);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_config_mqtt_port_cb(char* args)
{
    command_response_t ret = COMMAND_RESP_OK;
    unsigned argslen = strlen(args);
    if (argslen)
    {
        char* p = args;
        char* np;
        uint16_t port = strtol(p, &np, 10);
        if (p != np)
        {
            ret = COMMAND_RESP_OK;
            _at_wifi_ctx.mem->mqtt.port = port;
        }
        else
        {
            ret = COMMAND_RESP_ERR;
        }
    }
    log_out("PORT: %"PRIu16, _at_wifi_ctx.mem->mqtt.port);
    return ret;
}


static bool _at_wifi_config_setup_str2(char * str)
{
    static struct cmd_link_t cmds[] =
    {
        { "wifi_ssid",      "Set/get SSID",             _at_wifi_config_wifi_ssid_cb        , false , NULL },
        { "wifi_pwd",       "Set/get password",         _at_wifi_config_wifi_pwd_cb         , false , NULL },
        { "mqtt_addr",      "Set/get MQTT SSID",        _at_wifi_config_mqtt_addr_cb        , false , NULL },
        { "mqtt_user",      "Set/get MQTT user",        _at_wifi_config_mqtt_user_cb        , false , NULL },
        { "mqtt_pwd",       "Set/get MQTT password",    _at_wifi_config_mqtt_pwd_cb         , false , NULL },
        { "mqtt_ca",        "Set/get MQTT CA",          _at_wifi_config_mqtt_ca_cb          , false , NULL },
        { "mqtt_port",      "Set/get MQTT port",        _at_wifi_config_mqtt_port_cb        , false , NULL }
    };
    command_response_t r = COMMAND_RESP_ERR;
    if (str[0])
    {
    char * next = skip_to_space(str);
        if (next[0])
        {
            char * t = next;
            next = skip_space(next);
            *t = 0;
        }
        for(unsigned n=0; n < ARRAY_SIZE(cmds); n++)
        {
            struct cmd_link_t * cmd = &cmds[n];
            if (!strcmp(cmd->key, str))
                return cmd->cb(next);
        }
    }
    else r = COMMAND_RESP_OK;

    for(unsigned n=0; n < ARRAY_SIZE(cmds); n++)
    {
        struct cmd_link_t * cmd = &cmds[n];
        log_out("%10s : %s", cmd->key, cmd->desc);
    }
    return r;
}


void at_wifi_config_setup_str(char * str)
{
    _at_wifi_config_setup_str2(str);
}


bool at_wifi_get_id(char* str, uint8_t len)
{
    strncpy(str, _at_wifi_ctx.topic_header, len);
    return true;
}


static command_response_t _at_wifi_send_cb(char * args)
{
    char * pos = skip_space(args);
    return at_wifi_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_wifi_cmd_config_cb(char * args)
{
    bool ret = _at_wifi_config_setup_str2(skip_space(args));
    if (ret && _at_wifi_mem_is_valid())
    {
        _at_wifi_start();
    }
    return ret ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_wifi_cmd_j_cfg_cb(char* args)
{
    const uint32_t loop_timeout = 10;
    log_out(AT_WIFI_PRINT_CFG_JSON_HEADER);
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_WIFI_SSID(_at_wifi_ctx.mem->wifi.ssid));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_WIFI_PWD(_at_wifi_ctx.mem->wifi.pwd));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_MQTT_ADDR(_at_wifi_ctx.mem->mqtt.addr));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_MQTT_USER(_at_wifi_ctx.mem->mqtt.user));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_MQTT_PWD(_at_wifi_ctx.mem->mqtt.pwd));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_MQTT_CA(_at_wifi_ctx.mem->mqtt.ca));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_MQTT_PORT(_at_wifi_ctx.mem->mqtt.port));
    log_out_drain(loop_timeout);
    log_out(AT_WIFI_PRINT_CFG_JSON_TAIL);
    log_out_drain(loop_timeout);
    return COMMAND_RESP_OK;
}


command_response_t at_wifi_cmd_conn_cb(char* args)
{
    if (at_wifi_get_connected())
    {
        log_out("1 | Connected");
    }
    else
    {
        log_out("0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_dbg_cb(char* args)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_state_cb(char* args)
{
    log_out("State: %s(%u)", _at_wifi_get_state_str(_at_wifi_ctx.state), (unsigned)_at_wifi_ctx.state);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_send"  , "Send at_wifi message"    , _at_wifi_send_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"        , _at_wifi_dbg_cb           , false , NULL },
        { "comms_state" , "Get Comms state"         , _at_wifi_state_cb         , false , NULL },
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
    at_wifi_config->mqtt.port = 1883;
}


void at_wifi_config_init(comms_config_t* comms_config)
{
    comms_config->type = COMMS_TYPE_WIFI;
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
    if (!_at_wifi_ctx.time.sys ||
        since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.time.sys) > AT_WIFI_TS_TIMEOUT)
    {
        comms_debug("Old timestamp; invalid");
        comms_debug("_at_wifi_ctx.time.sys = %"PRIu32, _at_wifi_ctx.time.sys);
        comms_debug("diff = %"PRIu32" > %u", since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.time.sys), (unsigned)AT_WIFI_TS_TIMEOUT);
        _at_wifi_ctx.time.sys = 0;
        _at_wifi_ctx.time.ts_unix = 0;
        return false;
    }
    *ts = (int64_t)_at_wifi_ctx.time.ts_unix;
    _at_wifi_ctx.time.sys = 0;
    _at_wifi_ctx.time.ts_unix = 0;
    return true;
}
