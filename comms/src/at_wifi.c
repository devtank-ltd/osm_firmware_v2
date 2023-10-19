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

#ifndef DESIG_UNIQUE_ID0
#define DESIG_UNIQUE_ID0                        0x00C0FFEE
#endif // DESIG_UNIQUE_ID0


#define COMMS_DEFAULT_MTU                       256
#define COMMS_ID_STR                            "WIFI"

#define AT_WIFI_MAX_CMD_LEN                     256

#define AT_WIFI_MQTT_TOPIC_FMT                  "osm/%.*s/measurements"

#define AT_WIFI_SNTP_SERVER1                    "0.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER2                    "1.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER3                    "2.pool.ntp.org"  /* TODO */
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_SCHEME                     3   /* MQTT over TLS (verify server certificate) */
#define AT_WIFI_MQTT_CERT_KEY_ID                0
#define AT_WIFI_MQTT_CA_ID                      0
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_KEEP_ALIVE                 120 /* seconds */
#define AT_WIFI_MQTT_CLEAN_SESSION              0   /* enabled */
#define AT_WIFI_MQTT_LWT_MSG                    "{\"connection\": \"lost\"}"
#define AT_WIFI_MQTT_LWT_QOS                    0
#define AT_WIFI_MQTT_LWT_RETAIN                 0
#define AT_WIFI_MQTT_RECONNECT                  0
#define AT_WIFI_MQTT_QOS                        1
#define AT_WIFI_MQTT_RETAIN                     0


#define AT_WIFI_MQTT_TOPIC_LEN                  63
#define AT_WIFI_MQTT_TOPIC_MEASUREMENTS         "measurements"
#define AT_WIFI_MQTT_TOPIC_COMMAND              "cmd"
#define AT_WIFI_MQTT_TOPIC_CONNECTION           "conn"
#define AT_WIFI_MQTT_TOPIC_COMMAND_RESP         AT_WIFI_MQTT_TOPIC_COMMAND"/resp"

#define AT_WIFI_MQTT_RESP_PAYLOAD_LEN           63

#define AT_WIFI_TIMEOUT_MS_WIFI                 12000
#define AT_WIFI_TIMEOUT_MS_MQTT                 12000

#define AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS    30000
#define AT_WIFI_MQTT_FAIL_CONNECT_TIMEOUT_MS    30000


enum at_wifi_states_t
{
    AT_WIFI_STATE_OFF,
    AT_WIFI_STATE_WIFI_INIT,
    AT_WIFI_STATE_WIFI_CONNECTING,
    AT_WIFI_STATE_SNTP_WAIT_SET,
    AT_WIFI_STATE_MQTT_WAIT_USR_CONF,
    AT_WIFI_STATE_MQTT_WAIT_CONF,
    AT_WIFI_STATE_MQTT_CONNECTING,
    AT_WIFI_STATE_MQTT_WAIT_SUB,
    AT_WIFI_STATE_IDLE,
    AT_WIFI_STATE_MQTT_WAIT_PUB,
    AT_WIFI_STATE_MQTT_FAIL_CONNECT,
    AT_WIFI_STATE_WIFI_FAIL_CONNECT,
    AT_WIFI_STATE_WAIT_WIFI_STATE,
    AT_WIFI_STATE_WAIT_MQTT_STATE,
    AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE,
    AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE,
    AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE,
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
    char                    topic_header[AT_WIFI_MQTT_TOPIC_LEN + 1];
    char                    last_cmd[AT_WIFI_MAX_CMD_LEN];
    enum at_wifi_states_t   before_timedout_state;
    char                    before_timedout_last_cmd[AT_WIFI_MAX_CMD_LEN];
    at_wifi_config_t*       mem;
} _at_wifi_ctx =
{
    .state                      = AT_WIFI_STATE_OFF,
    .last_sent                  = 0,
    .topic_header               = "osm/unknown",
    .last_cmd                   = {0},
    .before_timedout_state      = AT_WIFI_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
};


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
            _at_wifi_str_is_valid_ascii(mem->mqtt.client_id , AT_WIFI_MQTT_CLIENTID_MAX_LEN , true  ) &&
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
    char* buf = _at_wifi_ctx.last_cmd;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, AT_WIFI_MAX_CMD_LEN - 3, fmt, args);
    va_end(args);
    buf[len] = 0;
    _at_wifi_ctx.last_sent = get_since_boot_ms();
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = '\n';
    buf[len+2] = 0;
    len = _at_wifi_raw_send(buf, len+2);
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
        _at_wifi_ctx.state = AT_WIFI_STATE_OFF;
        _at_wifi_printf("AT");
    }
}


static unsigned _at_wifi_mqtt_publish(char* topic, unsigned topic_len, char* message, unsigned message_len)
{
    unsigned len = 0;
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_IDLE:
            _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_PUB;
            topic_len = topic_len > AT_WIFI_MQTT_TOPIC_LEN ? AT_WIFI_MQTT_TOPIC_LEN : topic_len;
            len = _at_wifi_printf(
                "AT+MQTTPUB=%u,\"%.*s\",\"%.*s\",%u,%u",
                AT_WIFI_MQTT_LINK_ID,
                topic_len, topic,
                message_len, message,
                AT_WIFI_MQTT_QOS,
                AT_WIFI_MQTT_RETAIN
                );
            break;
        default:
            comms_debug("Wrong state to publish packet: %u", _at_wifi_ctx.state);
            break;
    }
    return len;
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


void at_wifi_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIx32, hex_arr[i]);
        uart_ring_out(COMMS_UART, buf, 2);
    }
    uart_ring_out(COMMS_UART, "\r\n", 2);
    on_comms_sent_ack(true);
}


void at_wifi_init(void)
{
    _at_wifi_ctx.mem = (at_wifi_config_t*)&persist_data.model_config;
    unsigned topic_header_len = snprintf(
        _at_wifi_ctx.topic_header,
        AT_WIFI_MQTT_TOPIC_LEN,
        "osm/%08"PRIX32,
        DESIG_UNIQUE_ID0
        );
    _at_wifi_ctx.topic_header[topic_header_len] = 0;
    _at_wifi_start();
}


void at_wifi_reset(void)
{
    _at_wifi_start();
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


static void _at_wifi_process_state_off(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_INIT;
        _at_wifi_printf("AT+CWINIT=1");
    }
}


static void _at_wifi_process_state_wifi_init(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
        _at_wifi_printf(
            "AT+CWJAP=%.*s,%.*s",
            AT_WIFI_MAX_SSID_LEN,   _at_wifi_ctx.mem->wifi.ssid,
            AT_WIFI_MAX_PWD_LEN,    _at_wifi_ctx.mem->wifi.pwd
            );
    }
}


static void _at_wifi_process_state_wifi_conn(char* msg, unsigned len)
{
    const char wifi_got_ip_msg[] = "WIFI GOT IP";
    const char error_msg[] = "ERROR";
    if (_at_wifi_is_str(wifi_got_ip_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_SNTP_WAIT_SET;
        _at_wifi_printf(
            "AT+CIPSNTPCFG=1,0,%s,%s,%s",
            AT_WIFI_SNTP_SERVER1,
            AT_WIFI_SNTP_SERVER2,
            AT_WIFI_SNTP_SERVER3
            );
    }
    else if (_at_wifi_is_str(error_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_FAIL_CONNECT;
    }
}


static void _at_wifi_do_mqtt_user_conf(void)
{
    _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_USR_CONF;
    _at_wifi_printf(
        "AT+MQTTUSERCFG=%u,%u,\"%.*s\",\"%.*s\",\"%.*s\",%u,%u,%.*s",
        AT_WIFI_MQTT_LINK_ID,
        AT_WIFI_MQTT_SCHEME,
        AT_WIFI_MQTT_CLIENTID_MAX_LEN,  _at_wifi_ctx.mem->mqtt.client_id,
        AT_WIFI_MQTT_USER_MAX_LEN,      _at_wifi_ctx.mem->mqtt.user,
        AT_WIFI_MQTT_PWD_MAX_LEN,       _at_wifi_ctx.mem->mqtt.pwd,
        AT_WIFI_MQTT_CERT_KEY_ID,
        AT_WIFI_MQTT_CA_ID,
        AT_WIFI_MQTT_CA_MAX_LEN,        _at_wifi_ctx.mem->mqtt.ca
        );
}


static void _at_wifi_process_state_sntp(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_do_mqtt_user_conf();
    }
}


static void _at_wifi_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_CONF;
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
    }
}


static void _at_wifi_process_state_mqtt_wait_conf(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_CONNECTING;
        _at_wifi_printf(
            "AT+MQTTCONN=%u,%.*s,%"PRIu16",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_ADDR_MAX_LEN, _at_wifi_ctx.mem->mqtt.addr,
            _at_wifi_ctx.mem->mqtt.port,
            AT_WIFI_MQTT_RECONNECT
            );
    }
}


static void _at_wifi_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_SUB;
        _at_wifi_printf(
            "AT+MQTTSUB=%u,\"%.%s\",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
            AT_WIFI_MQTT_QOS
            );
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
}


static void _at_wifi_do_command(char* payload, unsigned payload_len)
{
    char resp_topic[AT_WIFI_MQTT_TOPIC_LEN + 1];
    unsigned resp_topic_len = snprintf(
        resp_topic,
        AT_WIFI_MQTT_TOPIC_LEN,
        "%.*s/%s",
        AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
        AT_WIFI_MQTT_TOPIC_COMMAND_RESP
        );
    resp_topic[resp_topic_len] = 0;
    command_response_t ret_code = cmds_process(payload, payload_len);
    char resp_payload[AT_WIFI_MQTT_RESP_PAYLOAD_LEN + 1];
    unsigned resp_payload_len = snprintf(
        resp_payload,
        AT_WIFI_MQTT_RESP_PAYLOAD_LEN,
        "{\"ret_code\":0x%"PRIX8"}",
        (uint8_t)ret_code
        );
    resp_payload[resp_payload_len] = 0;
    if (!_at_wifi_mqtt_publish(resp_topic, resp_topic_len, resp_payload, resp_payload_len))
    {
        comms_debug("Failed to publish response packet");
    }
}


static void _at_wifi_parse_payload(char* topic, unsigned topic_len, char* payload, unsigned payload_len)
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
                return;
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
}


static void _at_wifi_parse_msg(char* msg, unsigned len)
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
        return;
    }
    if (np[0] != ',' || np[1] != '"')
    {
        return;
    }
    char* topic = np + 2;
    np = strchr(topic, '"');
    if (!np)
    {
        return;
    }
    unsigned topic_len = np - msg;
    if (topic_len > len)
    {
        return;
    }
    topic[topic_len] = 0;
    np = topic + topic_len + 1;
    if (np[0] != ',')
    {
        return;
    }
    p = np + 1;
    unsigned payload_length = strtoul(p, &np, 10);
    if (p == np)
    {
        return;
    }
    if (np[0] != ',')
    {
        return;
    }
    char* payload = np + 1;
    _at_wifi_parse_payload(topic, topic_len, payload, payload_length);
}


static void _at_wifi_process_state_idle(char* msg, unsigned len)
{
    const char recv_msg[] = "+MQTTSUBRECV:";
    const unsigned recv_msg_len = strlen(recv_msg);
    if (recv_msg_len <= len &&
        _at_wifi_is_str(recv_msg, msg, recv_msg_len))
    {
        /* Received message */
        char* msg_tail = msg + recv_msg_len;
        unsigned msg_tail_len = len - recv_msg_len;
        _at_wifi_parse_msg(msg_tail, msg_tail_len);
    }
}


static void _at_wifi_process_state_mqtt_wait_pub(char* msg, unsigned len)
{
    const char publish_ok_msg[] = "+MQTTPUB:OK";
    if (_at_wifi_is_str(publish_ok_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_IDLE;
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
            /* reset */
            _at_wifi_start();
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
            _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE;
            _at_wifi_printf("AT+MQTTCONN?");
        }
        else
        {
            /* reset */
            _at_wifi_start();
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
            /* TODO: retry */
            _at_wifi_retry_command();
        }
        else
        {
            /* reset */
            _at_wifi_start();
        }
    }
}


void at_wifi_process(char* msg)
{
    unsigned len = strlen(msg);
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_OFF:
            _at_wifi_process_state_off(msg, len);
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
        case AT_WIFI_STATE_TIMEDOUT_WIFI_WAIT_STATE:
            _at_wifi_process_state_timedout_wifi_wait_state(msg, len);
            break;
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE:
            _at_wifi_process_state_timedout_mqtt_wait_wifi_state(msg, len);
            break;
        case AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_MQTT_STATE:
            _at_wifi_process_state_timedout_mqtt_wait_mqtt_state(msg, len);
            break;
        default:
            break;
    }
}


bool at_wifi_get_connected(void)
{
    return _at_wifi_ctx.state == AT_WIFI_STATE_IDLE ||
           _at_wifi_ctx.state == AT_WIFI_STATE_MQTT_WAIT_PUB;
}


static void _at_wifi_timedout_start_wifi_status(void)
{
    _at_wifi_ctx.before_timedout_state = _at_wifi_ctx.state;
    strncpy(_at_wifi_ctx.before_timedout_last_cmd, _at_wifi_ctx.last_cmd, AT_WIFI_MAX_CMD_LEN);
    _at_wifi_printf("AT+CWSTATE?");
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
        _at_wifi_timedout_start_wifi_status();
        _at_wifi_ctx.state = AT_WIFI_STATE_TIMEDOUT_MQTT_WAIT_WIFI_STATE;
    }
}


static void _at_wifi_wifi_fail_connect(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_wifi_ctx.last_sent) > AT_WIFI_WIFI_FAIL_CONNECT_TIMEOUT_MS)
    {
        /* restart */
        _at_wifi_start();
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
    /* Check timeout */
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_OFF:
            /* resend AT? */
            break;
        case AT_WIFI_STATE_WIFI_INIT:
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


void at_wifi_config_setup_str(char * str)
{
    if (strstr(str, "dev-eui"))
    {
        log_out("Dev EUI: LINUX-DEV");
    }
    else if (strstr(str, "app-key"))
    {
        log_out("App Key: LINUX-APP");
    }
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


static command_response_t _at_wifi_config_cb(char * args)
{
    at_wifi_config_setup_str(skip_space(args));
    return COMMAND_RESP_OK;
}


static command_response_t _at_wifi_conn_cb(char* args)
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


struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "comms_send",   "Send at_wifi message",        _at_wifi_send_cb        , false , NULL },
                                       { "comms_config", "Set at_wifi config",          _at_wifi_config_cb      , false , NULL },
                                       { "comms_conn",   "LoRa connected",              _at_wifi_conn_cb        , false , NULL },
                                       { "comms_dbg",    "Comms Chip Debug",            _at_wifi_dbg_cb         , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
