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


#define COMMS_DEFAULT_MTU                       (1024 + 128)
#define COMMS_ID_STR                            "WIFI"

#define AT_WIFI_MAX_CMD_LEN                     (1024 + 128)

#define AT_WIFI_MQTT_TOPIC_FMT                  "osm/%.*s/measurements"

#define AT_WIFI_SNTP_SERVER1                    "0.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER2                    "1.pool.ntp.org"  /* TODO */
#define AT_WIFI_SNTP_SERVER3                    "2.pool.ntp.org"  /* TODO */
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_SCHEME                     1   /* MQTT over TLS (verify server certificate) */
#define AT_WIFI_MQTT_CERT_KEY_ID                0
#define AT_WIFI_MQTT_CA_ID                      0
#define AT_WIFI_MQTT_LINK_ID                    0
#define AT_WIFI_MQTT_KEEP_ALIVE                 120 /* seconds */
#define AT_WIFI_MQTT_CLEAN_SESSION              0   /* enabled */
#define AT_WIFI_MQTT_LWT_MSG                    "{\"connection\": \"lost\"}"
#define AT_WIFI_MQTT_LWT_QOS                    0
#define AT_WIFI_MQTT_LWT_RETAIN                 0
#define AT_WIFI_MQTT_RECONNECT                  0
#define AT_WIFI_MQTT_QOS                        0
#define AT_WIFI_MQTT_RETAIN                     0


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


enum at_wifi_states_t
{
    AT_WIFI_STATE_OFF,
    AT_WIFI_STATE_IS_CONNECTED,
    AT_WIFI_STATE_RESTORE,
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
    struct
    {
        char message[COMMS_DEFAULT_MTU];
        unsigned len;
    } publish_packet;
} _at_wifi_ctx =
{
    .state                      = AT_WIFI_STATE_OFF,
    .last_sent                  = 0,
    .topic_header               = "osm/unknown",
    .last_cmd                   = {0},
    .before_timedout_state      = AT_WIFI_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
    .publish_packet             = {.message = {0}, .len = 0},
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
        _at_wifi_ctx.state = AT_WIFI_STATE_IS_CONNECTED;
        _at_wifi_printf("AT+CWSTATE?");
    }
}


static void _at_wifi_retry_now_command(void)
{
    //_at_wifi_printf("%s", _at_wifi_ctx.last_cmd);
}


static unsigned _at_wifi_mqtt_publish(const char* topic, char* message, unsigned message_len)
{
    unsigned len = 0;
    switch (_at_wifi_ctx.state)
    {
        case AT_WIFI_STATE_IDLE:
        {
            _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_PUB;
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
            len = _at_wifi_printf(
                "AT+MQTTPUBRAW=%u,\"%.*s\",%u,%u,%u",
                AT_WIFI_MQTT_LINK_ID,
                full_topic_len, full_topic,
                message_len,
                AT_WIFI_MQTT_QOS,
                AT_WIFI_MQTT_RETAIN
                );
            //uint32_t start = get_since_boot_ms();
            //while (since_boot_delta(get_since_boot_ms(), start) < 70)
            //{
            //    uart_rings_in_drain();
            //}
            //comms_debug(" << %.*s", message_len, message);
            //_at_wifi_raw_send(message, message_len);
            //_at_wifi_ctx.state = AT_WIFI_STATE_MQTT_PUBLISHING;
            break;
        }
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


static bool _at_wifi_mqtt_publish_measurements(char* data, uint16_t len)
{
    return _at_wifi_mqtt_publish(AT_WIFI_MQTT_TOPIC_MEASUREMENTS, data, len) > 0;
}


void at_wifi_send(char* data, uint16_t len)
{
    if (!_at_wifi_mqtt_publish_measurements(data, len))
    {
        comms_debug("Failed to send measurement");
    }
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
    _at_wifi_ctx.state = AT_WIFI_STATE_OFF;
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
    if (_at_wifi_is_str(cwstate_msg, msg, cwstate_msg_len))
    {
        char* p, * np;
        unsigned len_rem = len - cwstate_msg_len;
        p = msg + cwstate_msg_len;
        uint8_t state = strtoul(p, &np, 10);
        bool can_skip = false;
        if (p != np && len_rem && np[0] == ',' && np[1] == '"' && msg[len] == '"')
        {
            if (state == AT_WIFI_CW_STATE_NO_IP ||
                state == AT_WIFI_CW_STATE_CONNECTED ||
                state == AT_WIFI_CW_STATE_CONNECTING)
            {
                char* ssid = np + 2;
                unsigned ssid_len = (msg + len) > (ssid + 1) ? msg + len - ssid - 1 : 0;
                /* Probably should check if saved has trailing spaces */
                char* cmp_ssid = _at_wifi_ctx.mem->wifi.ssid;
                unsigned cmp_ssid_len = strnlen(cmp_ssid, AT_WIFI_MAX_SSID_LEN);
                can_skip = can_skip &&
                    ssid_len == cmp_ssid_len &&
                    strncmp(cmp_ssid, ssid, cmp_ssid_len) == 0;
            }
        }
    }
    else if (_at_wifi_is_ok(msg, len))
    {
        if (can_skip)
        {
            _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
        }
        else
        {
            _at_wifi_ctx.state = AT_WIFI_STATE_RESTORE;
            _at_wifi_printf("AT+RESTORE");
        }
        can_skip = false;
    }
}


static void _at_wifi_process_state_restore(char* msg, unsigned len)
{
    const char ready[] = "ready";
    if (_at_wifi_is_str(ready, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_INIT;
        _at_wifi_printf("AT+CWINIT=1");
    }
}


static void _at_wifi_process_state_wifi_init(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_SETTING_MODE;
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_printf("AT+CWMODE=1");
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
}


static void at_wifi_process_state_wifi_setting_mode(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_CONNECTING;
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_printf(
            "AT+CWJAP=\"%.*s\",\"%.*s\"",
            AT_WIFI_MAX_SSID_LEN,   _at_wifi_ctx.mem->wifi.ssid,
            AT_WIFI_MAX_PWD_LEN,    _at_wifi_ctx.mem->wifi.pwd
            );
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
}


static void _at_wifi_process_state_wifi_conn(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_SNTP_WAIT_SET;
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_printf(
            "AT+CIPSNTPCFG=1,0,\"%s\",\"%s\",\"%s\"",
            AT_WIFI_SNTP_SERVER1,
            AT_WIFI_SNTP_SERVER2,
            AT_WIFI_SNTP_SERVER3
            );
    }
    else if (_at_wifi_is_str(error_msg, msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_WIFI_FAIL_CONNECT;
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
}


static void _at_wifi_do_mqtt_user_conf(void)
{
    _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_USR_CONF;
    _at_wifi_printf(
        "AT+MQTTUSERCFG=%u,%u,\"%.*s\",\"%.*s\",\"%.*s\",%u,%u,\"\"",
        AT_WIFI_MQTT_LINK_ID,
        AT_WIFI_MQTT_SCHEME,
        AT_WIFI_MQTT_CLIENTID_MAX_LEN,  _at_wifi_ctx.mem->mqtt.client_id,
        AT_WIFI_MQTT_USER_MAX_LEN,      _at_wifi_ctx.mem->mqtt.user,
        AT_WIFI_MQTT_PWD_MAX_LEN,       _at_wifi_ctx.mem->mqtt.pwd,
        AT_WIFI_MQTT_CERT_KEY_ID,
        AT_WIFI_MQTT_CA_ID
        );
}


static void _at_wifi_process_state_sntp(char* msg, unsigned len)
{
    const char time_updated_msg[] = "+TIME_UPDATED";
    if (_at_wifi_is_str(time_updated_msg, msg, len))
    {
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_do_mqtt_user_conf();
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
}


static void _at_wifi_process_state_mqtt_wait_conf(char* msg, unsigned len);

static void _at_wifi_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
{
    /*
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
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
    */
    _at_wifi_process_state_mqtt_wait_conf(msg, len);
}


static void _at_wifi_process_state_mqtt_wait_conf(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_CONNECTING;
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_printf(
            "AT+MQTTCONN=%u,\"%.*s\",%"PRIu16",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_ADDR_MAX_LEN, _at_wifi_ctx.mem->mqtt.addr,
            _at_wifi_ctx.mem->mqtt.port,
            AT_WIFI_MQTT_RECONNECT
            );
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
    }
}


static void _at_wifi_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_ctx.state = AT_WIFI_STATE_MQTT_WAIT_SUB;
        uint32_t start = get_since_boot_ms();
        while(since_boot_delta(get_since_boot_ms(), start) < 10);
        _at_wifi_printf(
            "AT+MQTTSUB=%u,\"%.*s\",%u",
            AT_WIFI_MQTT_LINK_ID,
            AT_WIFI_MQTT_TOPIC_LEN, _at_wifi_ctx.topic_header,
            AT_WIFI_MQTT_QOS
            );
    }
    else if (_at_wifi_is_error(msg, len))
    {
        _at_wifi_retry_now_command();
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
        _at_wifi_retry_now_command();
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
    //const char is_start_msg[] = ">";
    if (_at_wifi_is_ok(msg, len))
    {
        _at_wifi_raw_send(
            _at_wifi_ctx.publish_packet.message,
            _at_wifi_ctx.publish_packet.len
            );
    }
}


static void _at_wifi_process_state_mqtt_publishing(char* msg, unsigned len)
{
    const char pub_ok[] = ">+MQTTPUB:OK";
    if (_at_wifi_is_str(pub_ok, msg, len))
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
        case AT_WIFI_STATE_IS_CONNECTED:
            _at_wifi_process_state_is_connected(msg, len);
            break;
        case AT_WIFI_STATE_RESTORE:
            _at_wifi_process_state_restore(msg, len);
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
        case AT_WIFI_STATE_IS_CONNECTED:
            break;
        case AT_WIFI_STATE_RESTORE:
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
        /* Set SSID */
        strncpy(dest, src, max_dest_len);
        dest[max_dest_len] = 0;
    }
    /* Get SSID */
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


static command_response_t _at_wifi_config_mqtt_client_id_cb(char* args)
{
    _at_wifi_config_get_set_str(
        "CLID",
        _at_wifi_ctx.mem->mqtt.client_id,
        AT_WIFI_MQTT_CLIENTID_MAX_LEN,
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
        { "mqtt_client_id", "Set/get MQTT client id",   _at_wifi_config_mqtt_client_id_cb   , false , NULL },
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


static command_response_t _at_wifi_config_cb(char * args)
{
    bool ret = _at_wifi_config_setup_str2(skip_space(args));
    if (ret && _at_wifi_mem_is_valid())
    {
        _at_wifi_start();
    }
    return ret ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _at_wifi_print_config_cb(char* args)
{
    const unsigned max_padding = 15;
    const char str_fmt[] = "%*s: %.*s";
    log_out(str_fmt,
        max_padding, "WIFI SSID",
        AT_WIFI_MAX_SSID_LEN, _at_wifi_ctx.mem->wifi.ssid
        );
    log_out(str_fmt,
        max_padding, "WIFI PWD",
        AT_WIFI_MAX_PWD_LEN, _at_wifi_ctx.mem->wifi.pwd
        );
    log_out(str_fmt,
        max_padding, "MQTT ADDR",
        AT_WIFI_MQTT_ADDR_MAX_LEN, _at_wifi_ctx.mem->mqtt.addr
        );
    log_out(str_fmt,
        max_padding, "MQTT CLIENTID",
        AT_WIFI_MQTT_CLIENTID_MAX_LEN, _at_wifi_ctx.mem->mqtt.client_id
        );
    log_out(str_fmt,
        max_padding, "MQTT USER",
        AT_WIFI_MQTT_USER_MAX_LEN, _at_wifi_ctx.mem->mqtt.user
        );
    log_out(str_fmt,
        max_padding, "MQTT PWD",
        AT_WIFI_MQTT_PWD_MAX_LEN, _at_wifi_ctx.mem->mqtt.pwd
        );
    log_out(str_fmt,
        max_padding, "MQTT CA",
        AT_WIFI_MQTT_CA_MAX_LEN, _at_wifi_ctx.mem->mqtt.ca
        );
    log_out("%*s: %"PRIu16,
        max_padding, "MQTT PORT",
        _at_wifi_ctx.mem->mqtt.port
        );
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


static command_response_t _at_wifi_state_cb(char* args)
{
    const char* state_strs[] =
    {
        "OFF"                           ,
        "WIFI_INIT"                     ,
        "WIFI_SETTING_MODE"             ,
        "WIFI_CONNECTING"               ,
        "SNTP_WAIT_SET"                 ,
        "MQTT_WAIT_USR_CONF"            ,
        "MQTT_WAIT_CONF"                ,
        "MQTT_CONNECTING"               ,
        "MQTT_WAIT_SUB"                 ,
        "IDLE"                          ,
        "MQTT_WAIT_PUB"                 ,
        "MQTT_FAIL_CONNECT"             ,
        "WIFI_FAIL_CONNECT"             ,
        "WAIT_WIFI_STATE"               ,
        "WAIT_MQTT_STATE"               ,
        "TIMEDOUT_WIFI_WAIT_STATE"      ,
        "TIMEDOUT_MQTT_WAIT_WIFI_STATE" ,
        "TIMEDOUT_MQTT_WAIT_MQTT_STATE" ,
    };
    unsigned state = (unsigned)_at_wifi_ctx.state;
    log_out("State: %s(%u)", state_strs[state], state);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_send"  , "Send at_wifi message"    , _at_wifi_send_cb          , false , NULL },
        { "comms_config", "Set at_wifi config"      , _at_wifi_config_cb        , false , NULL },
        { "comms_pr_cfg", "Print at_wifi config"    , _at_wifi_print_config_cb  , false , NULL },
        { "comms_conn"  , "LoRa connected"          , _at_wifi_conn_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"        , _at_wifi_dbg_cb           , false , NULL },
        { "comms_state" , "Get Comms state"         , _at_wifi_state_cb         , false , NULL },
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


/* Return false if different
 *        true  if same      */
bool at_wifi_persist_config_cmp(void* d0, void* d1)
{
    return (
        d0 && d1 &&
        memcmp(d0, d1, sizeof(at_wifi_config_t)) == 0);
}
