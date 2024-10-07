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


#define COMMS_ID_STR                            "POE"


#define AT_POE_STATE_TIMEDOUT_INTERNET_WAIT_STATE   30000
#define AT_POE_TIMEOUT_MS_MQTT                      30000

#define AT_POE_WIFI_FAIL_CONNECT_TIMEOUT_MS         60000
#define AT_POE_MQTT_FAIL_CONNECT_TIMEOUT_MS         60000

#define AT_POE_STILL_OFF_TIMEOUT                    10000
#define AT_POE_TIMEOUT                              500
#define AT_POE_SNTP_TIMEOUT                         10000


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
    AT_POE_STATE_WAIT_MAC_ADDRESS,
    AT_POE_STATE_COUNT,
};


static struct
{
    enum at_poe_states_t    state;
    enum at_poe_states_t    before_timedout_state;
    char                    before_timedout_last_cmd[AT_BASE_MAX_CMD_LEN];
    at_poe_config_t*        mem;
    uint32_t                ip;
    at_mqtt_ctx_t           mqtt_ctx;
} _at_poe_ctx =
{
    .state                      = AT_POE_STATE_OFF,
    .before_timedout_state      = AT_POE_STATE_OFF,
    .before_timedout_last_cmd   = {0},
    .mem                        = NULL,
    .mqtt_ctx                   =
    {
        .mem                    = NULL,
        .topic_header               = "osm/unknown",
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
};
static struct cmd_link_t* _at_poe_config_cmds;


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
        "WAIT_MAC_ADDRESS"                              ,
    };
    _Static_assert(sizeof(state_strs)/sizeof(state_strs[0]) == AT_POE_STATE_COUNT, "Wrong number of states");
    unsigned _state = (unsigned)state;
    if (_state >= AT_POE_STATE_COUNT)
        return "<INVALID>";
    return state_strs[_state];
}


static bool _at_poe_mem_is_valid(void)
{
    return _at_poe_ctx.mem && at_mqtt_mem_is_valid();
}


static unsigned _at_poe_printf(const char* fmt, ...)
{
    comms_debug("Command when in state:%s", _at_poe_get_state_str(_at_poe_ctx.state));
    char* buf = _at_poe_ctx.mqtt_ctx.at_base_ctx.last_cmd.str;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, AT_BASE_MAX_CMD_LEN - 2, fmt, args);
    va_end(args);
    buf[len] = 0;
    _at_poe_ctx.mqtt_ctx.at_base_ctx.last_sent = get_since_boot_ms();
    comms_debug(" << %s", buf);
    buf[len] = '\r';
    buf[len+1] = 0;
    len = at_base_raw_send(buf, len+1);
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
    _at_poe_ctx.mqtt_ctx.at_base_ctx.off_since = get_since_boot_ms();
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
            message_len = message_len < COMMS_DEFAULT_MTU ? message_len : COMMS_DEFAULT_MTU;
            at_base_cmd_t* cmd = at_mqtt_publish_prep(topic, message, message_len);
            if (!cmd)
            {
                comms_debug("Failed to prep MQTT");
            }
            else
            {
                _at_poe_printf(cmd->str);
                _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_PUB;
                ret_len = message_len;
            }
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


bool at_poe_send_allowed(void)
{
    return false;
}


static bool _at_poe_mqtt_publish_measurements(char* data, uint16_t len)
{
    return _at_poe_mqtt_publish(AT_MQTT_TOPIC_MEASUREMENTS, data, len) > 0;
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
    static struct cmd_link_t config_cmds[] =
    {
        { "_",      "_",     NULL        , true , NULL },
    };
    struct cmd_link_t* tail = &config_cmds[ARRAY_SIZE(config_cmds)-1];
    at_mqtt_add_commands(tail);
    _at_poe_config_cmds = config_cmds;

    _at_poe_ctx.mem = (at_poe_config_t*)&persist_data.model_config.comms_config;

    _at_poe_ctx.mqtt_ctx.mem = &_at_poe_ctx.mem->mqtt;

    at_mqtt_init(&_at_poe_ctx.mqtt_ctx);
    _at_poe_ctx.state = AT_POE_STATE_OFF;
}


void at_poe_reset(void)
{
    _at_poe_reset();
}


static void _at_poe_process_state_off(char* msg, unsigned len)
{
    const char boot_message[] = "+ETH_GOT_IP:";
    const unsigned boot_message_len = strlen(boot_message);
    if (_at_poe_mem_is_valid() &&
        len >= boot_message_len &&
        is_str(boot_message, msg, boot_message_len))
    {
        _at_poe_printf("ATE0");
        _at_poe_ctx.state = AT_POE_STATE_DISABLE_ECHO;
    }
}


static void _at_poe_process_state_disable_echo(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        _at_poe_printf("AT+CIPETHMAC?");
        _at_poe_ctx.state = AT_POE_STATE_WAIT_MAC_ADDRESS;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_sntp(char* msg, unsigned len)
{
    const char time_updated_msg[] = "+TIME_UPDATED";
    if (is_str(time_updated_msg, msg, len))
    {
        _at_poe_printf("AT+MQTTCONN?");
        _at_poe_ctx.state = AT_POE_STATE_MQTT_IS_CONNECTED;
    }
    else if (at_base_is_error(msg, len))
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
    at_base_cmd_t* cmd = at_mqtt_get_mqtt_user_cfg();
    if (!cmd)
    {
        comms_debug("Failed to get MQTT user config");
    }
    else
    {
        _at_poe_printf(cmd->str);
        _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_USR_CONF;
    }
}


static void _at_poe_process_state_mqtt_is_connected(char* msg, unsigned len)
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
                _at_poe_printf("AT+MQTTSUB?");
                _at_poe_ctx.state = AT_POE_STATE_MQTT_IS_SUBSCRIBED;
                break;
            default:
                _at_poe_do_mqtt_user_conf();
                break;
        }
        conn = AT_MQTT_CONN_STATE_NOT_INIT;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_do_mqtt_sub(void)
{
    at_base_cmd_t* cmd = at_mqtt_get_mqtt_sub_cfg();
    if (!cmd)
    {
        comms_debug("Failed to get MQTT sub config.");
    }
    else
    {
        _at_poe_printf(cmd->str);
        _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_SUB;
    }
}


static void _at_poe_process_state_mqtt_wait_usr_conf(char* msg, unsigned len)
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
            _at_poe_printf(cmd->str);
            _at_poe_ctx.state = AT_POE_STATE_MQTT_WAIT_CONF;
        }
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_is_subscribed(char* msg, unsigned len)
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
            _at_poe_ctx.state = AT_POE_STATE_IDLE;
        }
        else
        {
            _at_poe_do_mqtt_sub();
        }
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_wait_conf(char* msg, unsigned len)
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
            _at_poe_printf(cmd->str);
            _at_poe_ctx.state = AT_POE_STATE_MQTT_CONNECTING;
        }

    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_connecting(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        _at_poe_sleep();
        _at_poe_do_mqtt_sub();
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static void _at_poe_process_state_mqtt_wait_sub(char* msg, unsigned len)
{
    const char already_subscribe_str[] = "ALREADY SUBSCRIBE";
    if (at_base_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
    else if (is_str(already_subscribe_str, msg, len))
    {
        /* Already subscribed, assume everything fine */
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
    else if (at_base_is_error(msg, len))
    {
        _at_poe_reset();
    }
}


static bool _at_poe_process_event(char* msg, unsigned len)
{
    char resp_payload[AT_MQTT_RESP_PAYLOAD_LEN + 1];
    int resp_payload_len = at_mqtt_process_event(msg, len, resp_payload, AT_MQTT_RESP_PAYLOAD_LEN + 1);
    return (AT_ERROR_CODE != resp_payload_len) &&
        _at_poe_mqtt_publish(AT_MQTT_TOPIC_COMMAND_RESP, resp_payload, resp_payload_len);
}


static void _at_poe_process_state_idle(char* msg, unsigned len)
{
}


static void _at_poe_process_state_mqtt_wait_pub(char* msg, unsigned len)
{
    if (at_base_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_MQTT_PUBLISHING;
        at_base_raw_send(
            _at_poe_ctx.mqtt_ctx.publish_packet.message,
            _at_poe_ctx.mqtt_ctx.publish_packet.len
            );
    }
    else if (at_base_is_error(msg, len))
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
    if (is_str(pub_ok, msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
        comms_debug("Successful send, propagating ACK");
        on_protocol_sent_ack(true);
    }
    else if (at_base_is_error(msg, len))
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


static void _at_poe_process_state_timedout_wait_mqtt_state(char* msg, unsigned len)
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
    if (is_str(sys_ts_msg, msg, sys_ts_msg_len))
    {
        _at_poe_ctx.mqtt_ctx.at_base_ctx.time.ts_unix = strtoull(&msg[sys_ts_msg_len], NULL, 10);
        _at_poe_ctx.mqtt_ctx.at_base_ctx.time.sys = get_since_boot_ms();
    }
    else if (at_base_is_ok(msg, len))
    {
        _at_poe_ctx.state = AT_POE_STATE_IDLE;
    }
}


static void _at_poe_process_state_wait_mac_address(char* msg, unsigned len)
{
    const char mac_msg[] = "+CIPETHMAC:\"";
    unsigned mac_msg_len = strlen(mac_msg);
    if (is_str(mac_msg, msg, mac_msg_len))
    {
        unsigned len_rem = len - mac_msg_len;
        const unsigned maxstrlen = AT_BASE_MAC_ADDRESS_LEN - 1;
        len_rem = len_rem > maxstrlen ? maxstrlen : len_rem;
        char* dest = _at_poe_ctx.mqtt_ctx.at_base_ctx.mac_address;
        memcpy(dest, msg + mac_msg_len, len_rem);
        dest[maxstrlen] = 0;
    }
    else if (at_base_is_ok(msg, len))
    {
        at_base_cmd_t* cmd = at_mqtt_get_ntp_cfg();
        if (!cmd)
        {
            comms_debug("Failed to get the NTP config");
        }
        else
        {
            _at_poe_printf(cmd->str);
            _at_poe_ctx.state = AT_POE_STATE_SNTP_WAIT_SET;
        }
    }
}


void at_poe_process(char* msg)
{
    unsigned len = strlen(msg);

    _at_poe_ctx.mqtt_ctx.at_base_ctx.last_recv = get_since_boot_ms();

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
        case AT_POE_STATE_WAIT_MAC_ADDRESS:
            _at_poe_process_state_wait_mac_address(msg, len);
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
           _at_poe_ctx.state == AT_POE_STATE_MQTT_PUBLISHING ||
           _at_poe_ctx.state == AT_POE_STATE_WAIT_TIMESTAMP;
}


static void _at_poe_check_mqtt_timeout(void)
{
    if (since_boot_delta(get_since_boot_ms(), _at_poe_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_POE_TIMEOUT_MS_MQTT)
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
    if (since_boot_delta(get_since_boot_ms(), _at_poe_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_POE_MQTT_FAIL_CONNECT_TIMEOUT_MS)
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
            uint32_t delta = since_boot_delta(now, _at_poe_ctx.mqtt_ctx.at_base_ctx.off_since);
            if (delta > AT_POE_STILL_OFF_TIMEOUT)
            {
                _at_poe_ctx.mqtt_ctx.at_base_ctx.off_since = now;
                _at_poe_start();
            }
            break;
        }
        case AT_POE_STATE_DISABLE_ECHO:
            /* fall through */
        case AT_POE_STATE_MQTT_IS_CONNECTED:
            if (since_boot_delta(now, _at_poe_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_POE_TIMEOUT)
            {
                _at_poe_reset();
            }
            break;
        case AT_POE_STATE_SNTP_WAIT_SET:
            if (since_boot_delta(now, _at_poe_ctx.mqtt_ctx.at_base_ctx.last_sent) > AT_POE_SNTP_TIMEOUT)
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


void at_poe_config_setup_str(char * str, cmd_ctx_t * ctx)
{
    at_base_config_setup_str(_at_poe_config_cmds, str, ctx);
}


bool at_poe_get_id(char* str, uint8_t len)
{
    return at_mqtt_get_id(str, len);
}


static command_response_t _at_poe_send_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = skip_space(args);
    return at_base_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t at_poe_cmd_config_cb(char * args, cmd_ctx_t * ctx)
{

    at_poe_config_t before_config;
    memcpy(&before_config, _at_poe_ctx.mem, sizeof(at_poe_config_t));
    command_response_t ret = at_base_config_setup_str(_at_poe_config_cmds, skip_space(args), ctx);
    if (_at_poe_mem_is_valid() && at_poe_persist_config_cmp(&before_config, _at_poe_ctx.mem))
    {
        _at_poe_start();
    }
    return ret;
}

static bool _at_poe_get_mac_address(char* buf, unsigned buflen);

command_response_t at_poe_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx)
{
    char mac_address[AT_BASE_MAC_ADDRESS_LEN];
    if (!_at_poe_get_mac_address(mac_address, AT_BASE_MAC_ADDRESS_LEN))
    {
        strncpy(mac_address, "UNKNOWN", AT_BASE_MAC_ADDRESS_LEN-1);
    }
    cmd_ctx_out(ctx,AT_BASE_PRINT_CFG_JSON_HEADER);
    cmd_ctx_flush(ctx);
    at_mqtt_cmd_j_cfg(ctx);
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_BASE_PRINT_CFG_JSON_MAC_ADDRESS(mac_address));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_BASE_PRINT_CFG_JSON_TAIL);
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
    at_base_raw_send(args, strlen(args));
    at_base_raw_send("\r\n", 2);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_boot_cb(char* args, cmd_ctx_t * ctx)
{
    at_base_boot(args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_poe_reset_cb(char* args, cmd_ctx_t * ctx)
{
    at_base_reset(args, ctx);
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
        { "comms_send"  , "Send at_poe message"         , _at_poe_send_cb          , false , NULL },
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


void at_poe_config_init(comms_config_t* comms_config)
{
    comms_config->type = COMMS_TYPE_POE;
    at_mqtt_config_init(&((at_poe_config_t*)comms_config)->mqtt);
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
    at_base_time_t* time = &_at_poe_ctx.mqtt_ctx.at_base_ctx.time;
    if (!time->sys ||
        since_boot_delta(get_since_boot_ms(), time->sys) > AT_POE_TS_TIMEOUT)
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


static bool _at_poe_get_mac_address(char* buf, unsigned buflen)
{
    char* src = _at_poe_ctx.mqtt_ctx.at_base_ctx.mac_address;
    const unsigned mac_len = AT_BASE_MAC_ADDRESS_LEN-1;
    if (strnlen(src, mac_len) != mac_len ||
        !str_is_valid_ascii(src, mac_len, true))
    {
        comms_debug("Don't have MAC address, retrieving");
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
