#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "at_poe.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "common.h"
#include "log.h"
#include "cmd.h"
#include "protocol.h"
#include "platform.h"

#ifndef DESIG_UNIQUE_ID0
#define DESIG_UNIQUE_ID0                        0x00C0FFEE
#endif // DESIG_UNIQUE_ID0


#define COMMS_DEFAULT_MTU                       (1024 + 128)
#define COMMS_ID_STR                            "POE"

#define AT_POE_MAX_CMD_LEN                      (1024 + 128)


#define AT_POE_SNTP_SERVER1                     "0.pool.ntp.org"  /* TODO */
#define AT_POE_SNTP_SERVER2                     "1.pool.ntp.org"  /* TODO */
#define AT_POE_SNTP_SERVER3                     "2.pool.ntp.org"  /* TODO */
#define AT_POE_MQTT_LINK_ID                     0
#define AT_POE_MQTT_CERT_KEY_ID                 0
#define AT_POE_MQTT_CA_ID                       0
#define AT_POE_MQTT_LINK_ID                     0
#define AT_POE_MQTT_KEEP_ALIVE                  120 /* seconds */
#define AT_POE_MQTT_CLEAN_SESSION               0   /* enabled */
#define AT_POE_MQTT_LWT_MSG                     "{\\\"connection\\\": \\\"lost\\\"}"
#define AT_POE_MQTT_LWT_QOS                     0
#define AT_POE_MQTT_LWT_RETAIN                  0
#define AT_POE_MQTT_RECONNECT                   0
#define AT_POE_MQTT_QOS                         0
#define AT_POE_MQTT_RETAIN                      0

enum at_poe_mqtt_scheme_t
{
    AT_POE_MQTT_SCHEME_BARE                                = 1,  /* MQTT over TCP. */
    AT_POE_MQTT_SCHEME_TLS_NO_CERT                         = 2,  /* MQTT over TLS (no certificate verify). */
    AT_POE_MQTT_SCHEME_TLS_VERIFY_SERVER                   = 3,  /* MQTT over TLS (verify server certificate). */
    AT_POE_MQTT_SCHEME_TLS_PROVIDE_CLIENT                  = 4,  /* MQTT over TLS (provide client certificate). */
    AT_POE_MQTT_SCHEME_TLS_VERIFY_SERVER_PROVIDE_CLIENT    = 5,  /* MQTT over TLS (verify server certificate and provide client certificate). */
    AT_POE_MQTT_SCHEME_WS                                  = 6,  /* MQTT over Websocket (TCP) */
    AT_POE_MQTT_SCHEME_WSS_NO_CERT                         = 7,  /* MQTT over Websocket Secure (TLS no certificate verify). */
    AT_POE_MQTT_SCHEME_WSS_VERIFY_SERVER                   = 8,  /* MQTT over Websocket Secure (TLS verify server certificate). */
    AT_POE_MQTT_SCHEME_WSS_PROVIDE_CLIENT                  = 9,  /* MQTT over Websocket Secure (TLS provide client certificate). */
    AT_POE_MQTT_SCHEME_WSS_VERIFY_SERVER_PROVIDE_CLIENT    = 10, /* MQTT over Websocket Secure (TLS verify server certificate and provide client certificate). */
    AT_POE_MQTT_SCHEME_COUNT,
};

#define AT_POE_MQTT_TOPIC_LEN                       63
#define AT_POE_MQTT_TOPIC_MEASUREMENTS              "measurements"
#define AT_POE_MQTT_TOPIC_COMMAND                   "cmd"
#define AT_POE_MQTT_TOPIC_CONNECTION                "conn"
#define AT_POE_MQTT_TOPIC_COMMAND_RESP              AT_POE_MQTT_TOPIC_COMMAND"/resp"

#define AT_POE_MQTT_RESP_PAYLOAD_LEN                63

#define AT_POE_STATE_TIMEDOUT_INTERNET_WAIT_STATE   30000
#define AT_POE_TIMEOUT_MS_MQTT                      30000

#define AT_POE_WIFI_FAIL_CONNECT_TIMEOUT_MS         60000
#define AT_POE_MQTT_FAIL_CONNECT_TIMEOUT_MS         60000

#define AT_POE_STILL_OFF_TIMEOUT                    10000
#define AT_POE_TIMEOUT                              500
#define AT_POE_SNTP_TIMEOUT                         10000


#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_ADDR                "    \"MQTT ADDR\": \"%.*s\","
#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_USER                "    \"MQTT USER\": \"%.*s\","
#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_PWD                 "    \"MQTT PWD\": \"%.*s\","
#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_SCHEME              "    \"MQTT SCHEME\": %"PRIu16","
#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_CA                  "    \"MQTT CA\": \"%.*s\","
#define AT_POE_PRINT_CFG_JSON_FMT_MQTT_PORT                "    \"MQTT PORT\": %"PRIu16""

#define AT_POE_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"AT "COMMS_ID_STR"\",\n\r  \"config\": {"
#define AT_POE_PRINT_CFG_JSON_MQTT_ADDR(_mqtt_addr)        AT_POE_PRINT_CFG_JSON_FMT_MQTT_ADDR    , AT_POE_MQTT_ADDR_MAX_LEN , _mqtt_addr
#define AT_POE_PRINT_CFG_JSON_MQTT_USER(_mqtt_user)        AT_POE_PRINT_CFG_JSON_FMT_MQTT_USER    , AT_POE_MQTT_USER_MAX_LEN , _mqtt_user
#define AT_POE_PRINT_CFG_JSON_MQTT_PWD(_mqtt_pwd)          AT_POE_PRINT_CFG_JSON_FMT_MQTT_PWD     , AT_POE_MQTT_PWD_MAX_LEN  , _mqtt_pwd
#define AT_POE_PRINT_CFG_JSON_MQTT_SCHEME(_mqtt_scheme)    AT_POE_PRINT_CFG_JSON_FMT_MQTT_SCHEME  , _mqtt_scheme
#define AT_POE_PRINT_CFG_JSON_MQTT_CA(_mqtt_ca)            AT_POE_PRINT_CFG_JSON_FMT_MQTT_CA      , AT_POE_MQTT_CA_MAX_LEN   , _mqtt_ca
#define AT_POE_PRINT_CFG_JSON_MQTT_PORT(_mqtt_port)        AT_POE_PRINT_CFG_JSON_FMT_MQTT_PORT    , _mqtt_port
#define AT_POE_PRINT_CFG_JSON_TAIL                         "  }\n\r}"


enum at_poe_states_t
{
    AT_POE_STATE_OFF,
    AT_POE_STATE_DISABLE_ECHO,
    AT_POE_STATE_SNTP_WAIT_SET,
    AT_POE_STATE_MQTT_IS_CONNECTED,
    AT_POE_STATE_MQTT_IS_SUBSCRIBED,
    AT_POE_STATE_MQTT_WAIT_USR_CONF,
    AT_POE_STATE_MQTT_WAIT_CONF,
    AT_POE_STATE_MQTT_CONNECTING,
    AT_POE_STATE_MQTT_WAIT_SUB,
    AT_POE_STATE_IDLE,
    AT_POE_STATE_MQTT_WAIT_PUB,
    AT_POE_STATE_MQTT_PUBLISHING,
    AT_POE_STATE_MQTT_FAIL_CONNECT,
    AT_POE_STATE_WAIT_MQTT_STATE,
    AT_POE_STATE_TIMEDOUT_WAIT_MQTT_STATE,
    AT_POE_STATE_WAIT_TIMESTAMP,
    AT_POE_STATE_COUNT,
};


enum at_poe_mqttconn_states_t
{
    AT_POE_MQTTCONN_STATE_NOT_INIT             = 0,
    AT_POE_MQTTCONN_STATE_SET_USR_CFG          = 1,
    AT_POE_MQTTCONN_STATE_SET_CONN_CFG         = 2,
    AT_POE_MQTTCONN_STATE_DISCONNECTED         = 3,
    AT_POE_MQTTCONN_STATE_CONN_EST             = 4,
    AT_POE_MQTTCONN_STATE_CONN_EST_NO_TOPIC    = 5,
    AT_POE_MQTTCONN_STATE_CONN_EST_WITH_TOPIC  = 6,
};


static struct
{
    enum at_poe_states_t    state;
    uint32_t                last_sent;
    uint32_t                last_recv;
    uint32_t                off_since;
    char                    topic_header[AT_POE_MQTT_TOPIC_LEN + 1];
    char                    last_cmd[AT_POE_MAX_CMD_LEN];
    enum at_poe_states_t    before_timedout_state;
    char                    before_timedout_last_cmd[AT_POE_MAX_CMD_LEN];
    at_poe_config_t*        mem;
    uint32_t                ip;
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
    port_n_pins_t           reset_pin;
    port_n_pins_t           boot_pin;
} _at_poe_ctx =
{
    .state                      = AT_POE_STATE_OFF,
    .last_sent                  = 0,
    .last_recv                  = 0,
    .off_since                  = 0,
    .topic_header               = "osm/unknown",
    .last_cmd                   = {0},
    .before_timedout_state      = AT_POE_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
    .publish_packet             = {.message = {0}, .len = 0},
    .time                       = {.ts_unix = 0, .sys = 0},
    .reset_pin                  = COMMS_RESET_PORT_N_PINS,
    .boot_pin                   = COMMS_BOOT_PORT_N_PINS,
};


static const char * _at_poe_get_state_str(enum at_poe_states_t state)
{
    static const char* state_strs[] =
    {
        "OFF"                                           ,
        "DISABLE_ECHO"                                  ,
        "SNTP_WAIT_SET"                                 ,
        "MQTT_IS_CONNECTED"                             ,
        "MQTT_IS_SUBSCRIBED"                            ,
        "MQTT_WAIT_USR_CONF"                            ,
        "MQTT_WAIT_CONF"                                ,
        "MQTT_CONNECTING"                               ,
        "MQTT_WAIT_SUB"                                 ,
        "IDLE"                                          ,
        "MQTT_WAIT_PUB"                                 ,
        "MQTT_PUBLISHING"                               ,
        "MQTT_FAIL_CONNECT"                             ,
        "WAIT_MQTT_STATE"                               ,
        "TIMEDOUT_MQTT_WAIT_MQTT_STATE"                 ,
        "WAIT_TIMESTAMP"                                ,
    };
    _Static_assert(sizeof(state_strs)/sizeof(state_strs[0]) == AT_POE_STATE_COUNT, "Wrong number of states");
    unsigned _state = (unsigned)state;
    if (_state >= AT_POE_STATE_COUNT)
        return "<INVALID>";
    return state_strs[_state];
}


static bool _at_poe_str_is_valid_ascii(char* str, unsigned max_len, bool required)
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


static bool _at_poe_mem_is_valid(void)
{
    at_poe_config_t* mem = _at_poe_ctx.mem;
    if (!mem)
    {
        return false;
    }
    return (_at_poe_str_is_valid_ascii(mem->mqtt.addr      , AT_POE_MQTT_ADDR_MAX_LEN     , true  ) &&
            _at_poe_str_is_valid_ascii(mem->mqtt.user      , AT_POE_MQTT_USER_MAX_LEN     , true  ) &&
            _at_poe_str_is_valid_ascii(mem->mqtt.pwd       , AT_POE_MQTT_PWD_MAX_LEN      , true  ) &&
            _at_poe_str_is_valid_ascii(mem->mqtt.ca        , AT_POE_MQTT_CA_MAX_LEN       , true  ) );
}


static unsigned _at_poe_raw_send(char* msg, unsigned len)
{
    return uart_ring_out(COMMS_UART, msg, len);
}


static unsigned _at_poe_printf(const char* fmt, ...)
{
    comms_debug("Command when in state:%s", _at_poe_get_state_str(_at_poe_ctx.state));
    char* buf = _at_poe_ctx.last_cmd;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, AT_POE_MAX_CMD_LEN - 2, fmt, args);
    va_end(args);
    buf[len] = 0;
    _at_poe_ctx.last_sent = get_since_boot_ms();
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = 0;
    len = _at_poe_raw_send(buf, len+1);
    return len;
}


static void _at_poe_start(void)
{
    if (!_at_poe_mem_is_valid())
    {
        comms_debug("Invalid memory");
    }
    else
    {
        /* Mem is valid */
        _at_poe_printf("AT+RST");
        _at_poe_ctx.state = AT_POE_STATE_OFF;
    }
}


static void _at_poe_reset(void)
{
    comms_debug("AT POE reset");
    _at_poe_ctx.off_since = get_since_boot_ms();
    _at_poe_printf("AT+RESTORE");
    _at_poe_ctx.state = AT_POE_STATE_OFF;
}


static unsigned _at_poe_mqtt_publish(const char* topic, char* message, unsigned message_len)
{
    unsigned ret_len = 0;
    switch (_at_poe_ctx.state)
    {
        case AT_POE_STATE_IDLE:
        {
            char full_topic[AT_POE_MQTT_TOPIC_LEN + 1];
            unsigned full_topic_len = snprintf(
                full_topic,
                AT_POE_MQTT_TOPIC_LEN,
                "%.*s/%s",
                AT_POE_MQTT_TOPIC_LEN, _at_poe_ctx.topic_header,
                topic
                );
            full_topic[full_topic_len] = 0;
            message_len = message_len < COMMS_DEFAULT_MTU ? message_len : COMMS_DEFAULT_MTU;
            strncpy(_at_poe_ctx.publish_packet.message, message, message_len);
            _at_poe_ctx.publish_packet.len = message_len;
            _at_poe_printf(
                "AT+MQTTPUBRAW=%u,\"%.*s\",%u,%u,%u",
                AT_POE_MQTT_LINK_ID,
                full_topic_len, full_topic,
                message_len,
                AT_POE_MQTT_QOS,
                AT_POE_MQTT_RETAIN
                );
            _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_PUB;
            ret_len = message_len;
            break;
        }
        default:
            comms_debug("Wrong state to publish packet: %u", _at_poe_ctx.state);
            break;
    }
    return ret_len;
}


uint16_t at_poe_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool at_poe_send_ready(void)
{
    return _at_poe_ctx.state == AT_POE_STATE_IDLE;
}


bool at_poe_send_str(char* str)
{
    if(!_at_poe_raw_send(str, strlen(str)))
        return false;
    return _at_poe_raw_send("\r\n", 2);
}


bool at_poe_send_allowed(void)
{
    return false;
}


static bool _at_poe_mqtt_publish_measurements(char* data, uint16_t len)
{
    return _at_poe_mqtt_publish(AT_POE_MQTT_TOPIC_MEASUREMENTS, data, len) > 0;
}


bool at_poe_send(char* data, uint16_t len)
{
    bool ret = _at_poe_mqtt_publish_measurements(data, len);
    if (!ret)
    {
        comms_debug("Failed to send measurement");
    }
    return ret;
}


void at_poe_init(void)
{
    platform_gpio_init(&_at_poe_ctx.reset_pin);
    platform_gpio_setup(&_at_poe_ctx.reset_pin, false, IO_PUPD_UP);
    platform_gpio_set(&_at_poe_ctx.reset_pin, true);
    platform_gpio_init(&_at_poe_ctx.boot_pin);
    platform_gpio_setup(&_at_poe_ctx.boot_pin, false, IO_PUPD_UP);
    platform_gpio_set(&_at_poe_ctx.boot_pin, true);


    _at_poe_ctx.mem = (at_poe_config_t*)&persist_data.model_config.comms_config;
    unsigned topic_header_len = snprintf(
        _at_poe_ctx.topic_header,
        AT_POE_MQTT_TOPIC_LEN,
        "osm/%08"PRIX32,
        DESIG_UNIQUE_ID0
        );
    _at_poe_ctx.topic_header[topic_header_len] = 0;
    comms_debug("MQTT Topic : %s", _at_poe_ctx.topic_header);
    _at_poe_ctx.state = AT_POE_STATE_OFF;
}


void at_poe_reset(void)
{
    _at_poe_reset();
}


static bool _at_poe_is_str(const char* ref, char* cmp, unsigned cmplen)
{
    const unsigned reflen = strlen(ref);
    return (reflen == cmplen && strncmp(ref, cmp, reflen) == 0);
}


static bool _at_poe_is_ok(char* msg, unsigned len)
{
    const char ok_msg[] = "OK";
    return _at_poe_is_str(ok_msg, msg, len);
}


static bool _at_poe_is_error(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    return _at_poe_is_str(error_msg, msg, len);
}


static void _at_poe_process_state_off(char* msg, unsigned len)
{
    const char boot_message[] = "+ETH_GOT_IP:";
    const unsigned boot_message_len = strlen(boot_message);
    if (_at_poe_mem_is_valid() &&
        len >= boot_message_len &&
        _at_poe_is_str(boot_message, msg, boot_message_len))
    {
        _at_poe_printf("ATE0");
        _at_poe_ctx.state = AT_POE_STATE_DISABLE_ECHO;
    }
}


static void _at_poe_process_state_disable_echo(char* msg, unsigned len)
{
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_printf(
            "AT+CIPSNTPCFG=1,0,\"%s\",\"%s\",\"%s\"",
            AT_POE_SNTP_SERVER1,
            AT_POE_SNTP_SERVER2,
            AT_POE_SNTP_SERVER3
            );
        _at_poe_ctx.state = AT_POE_STATE_SNTP_WAIT_SET;
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_sntp(char* msg, unsigned len)
{
    const char time_updated_msg[] = "+TIME_UPDATED";
    if (_at_poe_is_str(time_updated_msg, msg, len))
    {
        _at_poe_printf("AT+MQTTCONN?");
        _at_poe_ctx.state = AT_POE_STATE_MQTT_IS_CONNECTED;
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static bool _at_poe_sleep_loop_iterate(void *userdata)
{
    return false;
}


static void _at_poe_sleep(void)
{
    main_loop_iterate_for(10, _at_poe_sleep_loop_iterate, NULL);
}


static void _at_poe_do_mqtt_user_conf(void)
{

    enum at_poe_mqtt_scheme_t mqtt_scheme = _at_poe_ctx.mem->mqtt.scheme;
    if (!_at_poe_ctx.mem->mqtt.scheme || AT_POE_MQTT_SCHEME_COUNT <= _at_poe_ctx.mem->mqtt.scheme)
    {
        comms_debug("Invalid MQTT scheme, assuming bare (unless CA is set)");
        mqtt_scheme = AT_POE_MQTT_SCHEME_BARE;
    }

    const char * ca = _at_poe_ctx.mem->mqtt.ca;

    if (ca[0] && strcmp(ca,"--") /*Ignore -- as it's just a way of not wiping*/)
    {
        comms_debug("Using MQTT over SSL.");
        mqtt_scheme = AT_POE_MQTT_SCHEME_TLS_NO_CERT;
    }

    _at_poe_printf(
        "AT+MQTTUSERCFG=%u,%u,\"osm-0x%X\",\"%.*s\",\"%.*s\",%u,%u,\"\"",
        AT_POE_MQTT_LINK_ID,
        (unsigned)mqtt_scheme,
        DESIG_UNIQUE_ID0,
        AT_POE_MQTT_USER_MAX_LEN,      _at_poe_ctx.mem->mqtt.user,
        AT_POE_MQTT_PWD_MAX_LEN,       _at_poe_ctx.mem->mqtt.pwd,
        AT_POE_MQTT_CERT_KEY_ID,
        AT_POE_MQTT_CA_ID
        );
    _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_USR_CONF;
}


static bool _at_poe_parse_mqtt_conn(char* msg, unsigned len);


static void _at_poe_process_state_mqtt_is_connected(char* msg, unsigned len)
{
    static bool is_conn = false;
    const char mqtt_conn_msg[] = "+MQTTCONN:";
    unsigned mqtt_conn_msg_len = strlen(mqtt_conn_msg);
    if (_at_poe_is_str(mqtt_conn_msg, msg, mqtt_conn_msg_len))
    {
        char* conn_msg = msg + mqtt_conn_msg_len;
        unsigned conn_msg_len = len - mqtt_conn_msg_len;
        if (_at_poe_parse_mqtt_conn(conn_msg, conn_msg_len))
        {
            is_conn = true;
        }
    }
    else if (_at_poe_is_ok(msg, len))
    {
        if (is_conn)
        {
            is_conn = false;
            _at_poe_printf("AT+MQTTSUB?");
            _at_poe_ctx.state = AT_POE_STATE_MQTT_IS_SUBSCRIBED;
        }
        else
        {
            _at_poe_do_mqtt_user_conf();
        }
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_do_mqtt_sub(void)
{
    _at_poe_printf(
        "AT+MQTTSUB=%u,\"%.*s/"AT_POE_MQTT_TOPIC_COMMAND"\",%u",
        AT_POE_MQTT_LINK_ID,
        AT_POE_MQTT_TOPIC_LEN, _at_poe_ctx.topic_header,
        AT_POE_MQTT_QOS
        );
    _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_SUB;
}


static void _at_poe_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
{
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_printf(
            "AT+MQTTCONNCFG=%u,%u,%u,\"%.*s/%s\",\"%s\",%u,%u",
            AT_POE_MQTT_LINK_ID,
            AT_POE_MQTT_KEEP_ALIVE,
            AT_POE_MQTT_CLEAN_SESSION,
            AT_POE_MQTT_TOPIC_LEN, _at_poe_ctx.topic_header,
            AT_POE_MQTT_TOPIC_CONNECTION,
            AT_POE_MQTT_LWT_MSG,
            AT_POE_MQTT_LWT_QOS,
            AT_POE_MQTT_LWT_RETAIN
            );
        _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_CONF;
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static bool _at_poe_mqtt_topic_match(char* msg, unsigned len)
{
    char* p = msg;
    char* np;
    uint8_t link_id = strtoul(p, &np, 10);
    (void)link_id;

    char osm_topic[AT_POE_MQTT_TOPIC_LEN+1];
    snprintf(osm_topic, AT_POE_MQTT_TOPIC_LEN+1, "%.*s/%s", (int)(AT_POE_MQTT_TOPIC_LEN-strlen(AT_POE_MQTT_TOPIC_COMMAND)-1), _at_poe_ctx.topic_header, AT_POE_MQTT_TOPIC_COMMAND);
    osm_topic[AT_POE_MQTT_TOPIC_LEN] = '\0';
    if (p == np)
    {
        return false;
    }
    if (np[0] != ',')
    {
        comms_debug("Unexpected character found in MQTT topic.");
        return false;
    }
    char* curr_topic = strchr(p, '"');
    char* curr_topic_last = strchr(curr_topic + 1, '"');
    unsigned int curr_topic_len = curr_topic_last - curr_topic;
    if (curr_topic[curr_topic_len] != '"')
    {
        comms_debug("Unexpected last character in MQTT topic.");
        return false;
    }
    curr_topic_len--;
    if (curr_topic_len == strnlen(osm_topic, AT_POE_MQTT_TOPIC_LEN) &&
        strncmp(osm_topic, curr_topic + 1, curr_topic_len) == 0)
    {
        return true;
    }
    comms_debug("MQTT topics do not match.");
    return false;
}


static void _at_poe_process_state_mqtt_is_subscribed(char* msg, unsigned len)
{
    static bool is_subbed = false;
    const char mqtt_sub_msg[] = "+MQTTSUB:";
    const unsigned mqtt_sub_msg_len = strlen(mqtt_sub_msg);
    if (_at_poe_is_str(mqtt_sub_msg, msg, mqtt_sub_msg_len))
    {
        char* sub_msg = msg + mqtt_sub_msg_len;
        unsigned sub_msg_len = len - mqtt_sub_msg_len;
        if (_at_poe_mqtt_topic_match(sub_msg, sub_msg_len))
        {
            is_subbed = true;
        }
    }
    else if (_at_poe_is_ok(msg, len))
    {
        if (is_subbed)
        {
            is_subbed = false;
            _at_poe_ctx.state = AT_POE_STATE_IDLE;
        }
        else
        {
            _at_poe_do_mqtt_sub();
        }
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_wait_conf(char* msg, unsigned len)
{
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_sleep();
        _at_poe_printf(
            "AT+MQTTCONN=%u,\"%.*s\",%"PRIu16",%u",
            AT_POE_MQTT_LINK_ID,
            AT_POE_MQTT_ADDR_MAX_LEN, _at_poe_ctx.mem->mqtt.addr,
            _at_poe_ctx.mem->mqtt.port,
            AT_POE_MQTT_RECONNECT
            );
        _at_poe_ctx.state = AT_POE_STATE_MQTT_CONNECTING;
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_sleep();
        _at_poe_do_mqtt_sub();
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_wait_sub(char* msg, unsigned len)
{
    const char already_subscribe_str[] = "ALREADY SUBSCRIBE";
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
    else if (_at_poe_is_str(already_subscribe_str, msg, len))
    {
        /* Already subscribed, assume everything fine */
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_do_command(char* payload, unsigned payload_len)
{
    command_response_t ret_code = cmds_process(payload, payload_len, NULL);
    char resp_payload[AT_POE_MQTT_RESP_PAYLOAD_LEN + 1];
    unsigned resp_payload_len = snprintf(
        resp_payload,
        AT_POE_MQTT_RESP_PAYLOAD_LEN,
        "{\"ret_code\":0x%"PRIX8"}",
        (uint8_t)ret_code
        );
    resp_payload[resp_payload_len] = 0;
    if (!_at_poe_mqtt_publish(AT_POE_MQTT_TOPIC_COMMAND_RESP, resp_payload, resp_payload_len))
    {
        comms_debug("Failed to publish response packet");
    }
}


static bool _at_poe_parse_payload(char* topic, unsigned topic_len, char* payload, unsigned payload_len)
{
    /* Check for this sensor */
    unsigned own_topic_header_len = strnlen(_at_poe_ctx.topic_header, AT_POE_MQTT_TOPIC_LEN);
    if (topic_len >= own_topic_header_len &&
        strncmp(_at_poe_ctx.topic_header, topic, own_topic_header_len) == 0)
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
        if (_at_poe_is_str(AT_POE_MQTT_TOPIC_COMMAND, topic_tail, topic_tail_len))
        {
            /* Topic is command */
            _at_poe_do_command(payload, payload_len);
        }
    }
    /* leave room for potential broadcast messages */
    comms_debug("Command from OTA");
    return true;
}


static bool _at_poe_parse_msg(char* msg, unsigned len)
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
    return _at_poe_parse_payload(topic, topic_len, payload, payload_length);
}


static bool _at_poe_process_event(char* msg, unsigned len)
{
    bool r = false;
    const char recv_msg[] = "+MQTTSUBRECV:";
    const unsigned recv_msg_len = strlen(recv_msg);
    if (recv_msg_len <= len &&
        _at_poe_is_str(recv_msg, msg, recv_msg_len))
    {
        /* Received message */
        char* msg_tail = msg + recv_msg_len;
        unsigned msg_tail_len = len - recv_msg_len;
        r = _at_poe_parse_msg(msg_tail, msg_tail_len);
    }
    return r;
}


static void _at_poe_process_state_idle(char* msg, unsigned len)
{
}


static void _at_poe_process_state_mqtt_wait_pub(char* msg, unsigned len)
{
    if (_at_poe_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_MQTT_PUBLISHING;
        _at_poe_raw_send(
            _at_poe_ctx.publish_packet.message,
            _at_poe_ctx.publish_packet.len
            );
    }
    else if (_at_poe_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_publishing(char* msg, unsigned len)
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
    if (_at_poe_is_str(pub_ok, msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
        comms_debug("Successful send, propagating ACK");
        on_protocol_sent_ack(true);
    }
    else if (_at_poe_is_error(msg, len))
    {
        comms_debug("Failed send (ERROR), propagating NACK");
        on_protocol_sent_ack(false);
        _at_poe_reset();
    }
}


static void _at_poe_retry_command(void)
{
    _at_poe_printf("%s", _at_poe_ctx.before_timedout_last_cmd);
    _at_poe_ctx.state = _at_poe_ctx.before_timedout_state;
}


static bool _at_poe_parse_mqtt_conn(char* msg, unsigned len)
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
    return conn == AT_POE_MQTTCONN_STATE_CONN_EST_WITH_TOPIC;
}


static void _at_poe_process_state_timedout_wait_mqtt_state(char* msg, unsigned len)
{
    const char mqtt_conn_msg[] = "+MQTTCONN:";
    unsigned mqtt_conn_msg_len = strlen(mqtt_conn_msg);
    if (_at_poe_is_str(mqtt_conn_msg, msg, mqtt_conn_msg_len))
    {
        char* conn_msg = msg + mqtt_conn_msg_len;
        unsigned conn_msg_len = len - mqtt_conn_msg_len;
        if (_at_poe_parse_mqtt_conn(conn_msg, conn_msg_len))
        {
            _at_poe_retry_command();
        }
        else
        {
            _at_poe_reset();
        }
    }
}


static void _at_poe_process_state_wait_timestamp(char* msg, unsigned len)
{
    const char sys_ts_msg[] = "+SYSTIMESTAMP:";
    unsigned sys_ts_msg_len = strlen(sys_ts_msg);
    if (_at_poe_is_str(sys_ts_msg, msg, sys_ts_msg_len))
    {
        _at_poe_ctx.time.ts_unix = strtoull(&msg[sys_ts_msg_len], NULL, 10);
        _at_poe_ctx.time.sys = get_since_boot_ms();
    }
    else if (_at_poe_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
}


void at_poe_process(char* msg)
{
    unsigned len = strlen(msg);

    _at_poe_ctx.last_recv = get_since_boot_ms();

    comms_debug("Message when in state:%s", _at_poe_get_state_str(_at_poe_ctx.state));

    if (_at_poe_process_event(msg, len))
    {
        return;
    }

    switch (_at_poe_ctx.state)
    {
        case AT_POE_STATE_OFF:
            _at_poe_process_state_off(msg, len);
            break;
        case AT_POE_STATE_DISABLE_ECHO:
            _at_poe_process_state_disable_echo(msg, len);
            break;
        case AT_POE_STATE_SNTP_WAIT_SET:
            _at_poe_process_state_sntp(msg, len);
            break;
        case AT_POE_STATE_MQTT_IS_CONNECTED:
            _at_poe_process_state_mqtt_is_connected(msg, len);
            break;
        case AT_POE_STATE_MQTT_IS_SUBSCRIBED:
            _at_poe_process_state_mqtt_is_subscribed(msg, len);
            break;
        case AT_POE_STATE_MQTT_WAIT_USR_CONF:
            _at_poe_process_state_mqtt_wait_usr_conf(msg, len);
            break;
        case AT_POE_STATE_MQTT_WAIT_CONF:
            _at_poe_process_state_mqtt_wait_conf(msg, len);
            break;
        case AT_POE_STATE_MQTT_CONNECTING:
            _at_poe_process_state_mqtt_connecting(msg, len);
            break;
        case AT_POE_STATE_MQTT_WAIT_SUB:
            _at_poe_process_state_mqtt_wait_sub(msg, len);
            break;
        case AT_POE_STATE_IDLE:
            _at_poe_process_state_idle(msg, len);
            break;
        case AT_POE_STATE_MQTT_WAIT_PUB:
            _at_poe_process_state_mqtt_wait_pub(msg, len);
            break;
        case AT_POE_STATE_MQTT_PUBLISHING:
            _at_poe_process_state_mqtt_publishing(msg, len);
            break;
        case AT_POE_STATE_TIMEDOUT_WAIT_MQTT_STATE:
            _at_poe_process_state_timedout_wait_mqtt_state(msg, len);
            break;
        case AT_POE_STATE_WAIT_TIMESTAMP:
            _at_poe_process_state_wait_timestamp(msg, len);
            break;
        default:
            break;
    }
}


bool at_poe_get_connected(void)
{
    return _at_poe_ctx.state == AT_POE_STATE_IDLE ||
           _at_poe_ctx.state == AT_POE_STATE_MQTT_WAIT_PUB ||
           _at_poe_ctx.state == AT_POE_STATE_WAIT_TIMESTAMP;
}


static void _at_poe_check_mqtt_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_poe_ctx.last_sent) > AT_POE_TIMEOUT_MS_MQTT)
    {
        if (_at_poe_ctx.state == AT_POE_STATE_MQTT_PUBLISHING)
        {
            comms_debug("Failed send (TIMEOUT), propagating NACK");
            on_protocol_sent_ack(false);
        }
        _at_poe_printf("AT+MQTTCONN?");
        _at_poe_ctx.state = AT_POE_STATE_TIMEDOUT_WAIT_MQTT_STATE;
    }
}


static void _at_poe_mqtt_fail_connect(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_poe_ctx.last_sent) > AT_POE_MQTT_FAIL_CONNECT_TIMEOUT_MS)
    {
        /* restart MQTT config */
        _at_poe_do_mqtt_user_conf();
    }
}


void at_poe_loop_iteration(void)
{
    uint32_t now = get_since_boot_ms();

    /* Check timeout */
    switch (_at_poe_ctx.state)
    {
        case AT_POE_STATE_OFF:
        {
            uint32_t delta = since_boot_delta(now, _at_poe_ctx.off_since);
            if (delta > AT_POE_STILL_OFF_TIMEOUT)
            {
                _at_poe_ctx.off_since = now;
                _at_poe_start();
            }
            break;
        }
        case AT_POE_STATE_DISABLE_ECHO:
            /* fall through */
        case AT_POE_STATE_MQTT_IS_CONNECTED:
            if (since_boot_delta(now, _at_poe_ctx.last_sent) > AT_POE_TIMEOUT)
            {
                _at_poe_reset();
            }
            break;
        case AT_POE_STATE_SNTP_WAIT_SET:
            if (since_boot_delta(now, _at_poe_ctx.last_sent) > AT_POE_SNTP_TIMEOUT)
            {
                _at_poe_reset();
            }
            break;
        case AT_POE_STATE_MQTT_WAIT_USR_CONF:
            /* fall through */
        case AT_POE_STATE_MQTT_WAIT_CONF:
            /* fall through */
        case AT_POE_STATE_MQTT_CONNECTING:
            /* fall through */
        case AT_POE_STATE_MQTT_WAIT_SUB:
            /* fall through */
        case AT_POE_STATE_MQTT_WAIT_PUB:
            /* fall through */
        case AT_POE_STATE_MQTT_PUBLISHING:
            _at_poe_check_mqtt_timeout();
            break;
        case AT_POE_STATE_MQTT_FAIL_CONNECT:
            _at_poe_mqtt_fail_connect();
        default:
            break;
    }
}


static void _at_poe_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx)
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


static command_response_t _at_poe_config_mqtt_addr_cb(char* args, cmd_ctx_t * ctx)
{
    _at_poe_config_get_set_str(
        "ADDR",
        _at_poe_ctx.mem->mqtt.addr,
        AT_POE_MQTT_ADDR_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_config_mqtt_user_cb(char* args, cmd_ctx_t * ctx)
{
    _at_poe_config_get_set_str(
        "USER",
        _at_poe_ctx.mem->mqtt.user,
        AT_POE_MQTT_USER_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_config_mqtt_pwd_cb(char* args, cmd_ctx_t * ctx)
{
    _at_poe_config_get_set_str(
        "PWD",
        _at_poe_ctx.mem->mqtt.pwd,
        AT_POE_MQTT_PWD_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static bool _at_poe_config_get_set_u16(const char* name, uint16_t* dest, char* src, cmd_ctx_t * ctx)
{
    bool ret = true;
    unsigned len = strlen(src);
    if (len && dest)
    {
        char* np, * p = src;
        uint16_t new_uint16 = strtol(p, &np, 10);
        if (p != np)
        {
            *dest = new_uint16;
        }
        else
        {
            ret = false;
        }
    }
    /* Get */
    cmd_ctx_out(ctx,"%s: %"PRIu16, name, *dest);
    return ret;
}


static command_response_t _at_poe_config_mqtt_scheme_cb(char* args, cmd_ctx_t * ctx)
{
    return _at_poe_config_get_set_u16(
        "SCHEME",
        &_at_poe_ctx.mem->mqtt.scheme,
        args, ctx) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _at_poe_config_mqtt_ca_cb(char* args, cmd_ctx_t * ctx)
{
    _at_poe_config_get_set_str(
        "CA",
        _at_poe_ctx.mem->mqtt.ca,
        AT_POE_MQTT_CA_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_config_mqtt_port_cb(char* args, cmd_ctx_t * ctx)
{
    return _at_poe_config_get_set_u16(
        "PORT",
        &_at_poe_ctx.mem->mqtt.port,
        args, ctx) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static bool _at_poe_config_setup_str2(char * str, cmd_ctx_t * ctx)
{
    static struct cmd_link_t cmds[] =
    {
        { "mqtt_addr",      "Set/get MQTT SSID",        _at_poe_config_mqtt_addr_cb        , false , NULL },
        { "mqtt_user",      "Set/get MQTT user",        _at_poe_config_mqtt_user_cb        , false , NULL },
        { "mqtt_pwd",       "Set/get MQTT password",    _at_poe_config_mqtt_pwd_cb         , false , NULL },
        { "mqtt_sch",       "Set/get MQTT scheme",      _at_poe_config_mqtt_scheme_cb      , false , NULL },
        { "mqtt_ca",        "Set/get MQTT CA",          _at_poe_config_mqtt_ca_cb          , false , NULL },
        { "mqtt_port",      "Set/get MQTT port",        _at_poe_config_mqtt_port_cb        , false , NULL }
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
                return cmd->cb(next, ctx);
        }
    }
    else r = COMMAND_RESP_OK;

    for(unsigned n=0; n < ARRAY_SIZE(cmds); n++)
    {
        struct cmd_link_t * cmd = &cmds[n];
        cmd_ctx_out(ctx,"%10s : %s", cmd->key, cmd->desc);
    }
    return r;
}


void at_poe_config_setup_str(char * str, cmd_ctx_t * ctx)
{
    _at_poe_config_setup_str2(str, ctx);
}


bool at_poe_get_id(char* str, uint8_t len)
{
    strncpy(str, _at_poe_ctx.topic_header, len);
    return true;
}


static command_response_t _at_poe_send_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = skip_space(args);
    return at_poe_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_poe_cmd_config_cb(char * args, cmd_ctx_t * ctx)
{
    bool ret = _at_poe_config_setup_str2(skip_space(args), ctx);
    if (ret && _at_poe_mem_is_valid())
    {
        _at_poe_start();
    }
    return ret ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_poe_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_HEADER);
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_ADDR(_at_poe_ctx.mem->mqtt.addr));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_USER(_at_poe_ctx.mem->mqtt.user));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_PWD(_at_poe_ctx.mem->mqtt.pwd));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_SCHEME(_at_poe_ctx.mem->mqtt.scheme));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_CA(_at_poe_ctx.mem->mqtt.ca));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_MQTT_PORT(_at_poe_ctx.mem->mqtt.port));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_POE_PRINT_CFG_JSON_TAIL);
    cmd_ctx_flush(ctx);
    return COMMAND_RESP_OK;
}


command_response_t at_poe_cmd_conn_cb(char* args, cmd_ctx_t * ctx)
{
    if (at_poe_get_connected())
    {
        cmd_ctx_out(ctx,"1 | Connected");
    }
    else
    {
        cmd_ctx_out(ctx,"0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_boot_cb(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_poe_ctx.boot_pin, is_out);
    cmd_ctx_out(ctx,"BOOT PIN: %u", is_out ? 1U : 0U);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_reset_cb(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_poe_ctx.reset_pin, is_out);
    cmd_ctx_out(ctx,"RESET PIN: %u", is_out ? 1U : 0U);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_state_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"State: %s(%u)", _at_poe_get_state_str(_at_poe_ctx.state), (unsigned)_at_poe_ctx.state);
    return COMMAND_RESP_OK;
}

static command_response_t _at_poe_restart_cb(char* args, cmd_ctx_t * ctx) {
    _at_poe_reset();
    return COMMAND_RESP_OK;
}


struct cmd_link_t* at_poe_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_send"  , "Send at_poe message"        , _at_poe_send_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"            , _at_poe_dbg_cb           , false , NULL },
        { "comms_boot",   "Enable/disable boot line"    , _at_poe_boot_cb          , false , NULL },
        { "comms_reset",  "Enable/disable reset line"   , _at_poe_reset_cb         , false , NULL },
        { "comms_state" , "Get Comms state"             , _at_poe_state_cb         , false , NULL },
        { "comms_restart", "Comms restart"              , _at_poe_restart_cb       , false , NULL }
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


/* Return true  if different
 *        false if same      */
bool at_poe_persist_config_cmp(void* d0, void* d1)
{
    return !(
        d0 && d1 &&
        memcmp(d0, d1, sizeof(at_poe_config_t)) == 0);
}


static void _at_poe_config_init2(at_poe_config_t* at_poe_config)
{
    memset(at_poe_config, 0, sizeof(at_poe_config_t));
    at_poe_config->mqtt.scheme = AT_POE_MQTT_SCHEME_BARE;
    at_poe_config->mqtt.port = 1883;
}


void at_poe_config_init(comms_config_t* comms_config)
{
    comms_config->type = COMMS_TYPE_WIFI;
    _at_poe_config_init2((at_poe_config_t*)comms_config);
}


/*                  TIMESTAMP RETRIEVAL                  */
#define AT_POE_TS_TIMEOUT                      50


static bool _at_poe_ts_loop_iteration(void* userdata)
{
    return _at_poe_ctx.state != AT_POE_STATE_WAIT_TIMESTAMP;
}


bool at_poe_get_unix_time(int64_t * ts)
{
    if (!ts || _at_poe_ctx.state != AT_POE_STATE_IDLE)
    {
        return false;
    }
    _at_poe_printf("AT+SYSTIMESTAMP?");
    _at_poe_ctx.state = AT_POE_STATE_WAIT_TIMESTAMP;
    if (!main_loop_iterate_for(AT_POE_TS_TIMEOUT, _at_poe_ts_loop_iteration, NULL))
    {
        /* Timed out - Not sure what to do with the chip really... */
        comms_debug("Timed out");
        return false;
    }
    if (!_at_poe_ctx.time.sys ||
        since_boot_delta(get_since_boot_ms(), _at_poe_ctx.time.sys) > AT_POE_TS_TIMEOUT)
    {
        comms_debug("Old timestamp; invalid");
        comms_debug("_at_poe_ctx.time.sys = %"PRIu32, _at_poe_ctx.time.sys);
        comms_debug("diff = %"PRIu32" > %u", since_boot_delta(get_since_boot_ms(), _at_poe_ctx.time.sys), (unsigned)AT_POE_TS_TIMEOUT);
        _at_poe_ctx.time.sys = 0;
        _at_poe_ctx.time.ts_unix = 0;
        return false;
    }
    *ts = (int64_t)_at_poe_ctx.time.ts_unix;
    _at_poe_ctx.time.sys = 0;
    _at_poe_ctx.time.ts_unix = 0;
    return true;
}
