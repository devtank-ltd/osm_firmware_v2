#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "at_mqtt.h"
#include "common.h"
#include "log.h"
#include "cmd.h"

#ifndef DESIG_UNIQUE_ID0
#define DESIG_UNIQUE_ID0                        0x00C0FFEE
#endif // DESIG_UNIQUE_ID0

#define AT_MQTT_MAX_CMD_LEN                      (1024 + 128)

#define AT_MQTT_SNTP_SERVER1                        "0.pool.ntp.org"  /* TODO */
#define AT_MQTT_SNTP_SERVER2                        "1.pool.ntp.org"  /* TODO */
#define AT_MQTT_SNTP_SERVER3                        "2.pool.ntp.org"  /* TODO */
#define AT_MQTT_LINK_ID                             0
#define AT_MQTT_CERT_KEY_ID                         0
#define AT_MQTT_CA_ID                               0
#define AT_MQTT_KEEP_ALIVE                          120 /* seconds */
#define AT_MQTT_CLEAN_SESSION                       0   /* enabled */
#define AT_MQTT_LWT_MSG                             "{\\\"connection\\\": \\\"lost\\\"}"
#define AT_MQTT_LWT_QOS                             0
#define AT_MQTT_LWT_RETAIN                          0
#define AT_MQTT_RECONNECT                           0
#define AT_MQTT_QOS                                 0
#define AT_MQTT_RETAIN                              0


static at_mqtt_ctx_t* _at_mqtt_ctx = NULL;


bool at_mqtt_mem_is_valid(void)
{
    if (!_at_mqtt_ctx)
    {
        comms_debug("Not given context");
        return false;
    }
    else if (!_at_mqtt_ctx->mem)
    {
        comms_debug("Not given memory");
        return false;
    }
    return (str_is_valid_ascii(_at_mqtt_ctx->mem->addr  , AT_MQTT_ADDR_MAX_LEN       , true  ) &&
            str_is_valid_ascii(_at_mqtt_ctx->mem->user  , AT_MQTT_USER_MAX_LEN       , true  ) &&
            str_is_valid_ascii(_at_mqtt_ctx->mem->pwd   , AT_MQTT_PWD_MAX_LEN        , true  ) &&
            str_is_valid_ascii(_at_mqtt_ctx->mem->ca    , AT_MQTT_CA_MAX_LEN         , true  ) );
}


at_esp_cmd_t* at_mqtt_publish_prep(const char* topic, char* message, unsigned message_len)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        char full_topic[AT_MQTT_TOPIC_LEN + 1];
        unsigned full_topic_len = snprintf(
            full_topic,
            AT_MQTT_TOPIC_LEN,
            "%.*s/%s",
            AT_MQTT_TOPIC_LEN, _at_mqtt_ctx->topic_header,
            topic
            );
        full_topic[full_topic_len] = 0;
        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+MQTTPUBRAW=%u,\"%.*s\",%u,%u,%u",
            AT_MQTT_LINK_ID,
            full_topic_len, full_topic,
            message_len,
            AT_MQTT_QOS,
            AT_MQTT_RETAIN
            );
        ret->str[ret->len] = 0;
        strncpy(_at_mqtt_ctx->publish_packet.message, message, message_len);
        _at_mqtt_ctx->publish_packet.len = message_len;
    }
    else
    {
        comms_debug("Not given context");
    }
    return ret;
}


void at_mqtt_init(at_mqtt_ctx_t* ctx)
{
    if (ctx)
    {
        _at_mqtt_ctx = ctx;
        at_esp_init(&ctx->at_esp_ctx);
        unsigned topic_header_len = snprintf(
            _at_mqtt_ctx->topic_header,
            AT_MQTT_TOPIC_LEN,
            "osm/%08"PRIX32,
            DESIG_UNIQUE_ID0
            );
        _at_mqtt_ctx->topic_header[topic_header_len] = 0;
        comms_debug("MQTT Topic : %s", _at_mqtt_ctx->topic_header);
    }
    else
    {
        comms_debug("Handed NULL pointer for context.");
    }
}


at_esp_cmd_t* at_mqtt_get_ntp_cfg(void)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+CIPSNTPCFG=1,0,\"%s\",\"%s\",\"%s\"",
            AT_MQTT_SNTP_SERVER1,
            AT_MQTT_SNTP_SERVER2,
            AT_MQTT_SNTP_SERVER3
            );
        ret->str[ret->len] = 0;
    }
    return ret;
}


at_esp_cmd_t* at_mqtt_get_mqtt_user_cfg(void)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        enum at_mqtt_scheme_t mqtt_scheme = _at_mqtt_ctx->mem->scheme;
        if (!_at_mqtt_ctx->mem->scheme || AT_MQTT_SCHEME_COUNT <= _at_mqtt_ctx->mem->scheme)
        {
            comms_debug("Invalid MQTT scheme, assuming Websocket no cert");
            mqtt_scheme = AT_MQTT_SCHEME_WSS_NO_CERT;
        }

        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+MQTTUSERCFG=%u,%u,\"osm-0x%"PRIX32"\",\"%.*s\",\"%.*s\",%u,%u,\"\"",
            AT_MQTT_LINK_ID,
            (unsigned)mqtt_scheme,
            (uint32_t)DESIG_UNIQUE_ID0,
            AT_MQTT_USER_MAX_LEN,      _at_mqtt_ctx->mem->user,
            AT_MQTT_PWD_MAX_LEN,       _at_mqtt_ctx->mem->pwd,
            AT_MQTT_CERT_KEY_ID,
            AT_MQTT_CA_ID
            );
        ret->str[ret->len] = 0;
    }
    return ret;
}


at_esp_cmd_t* at_mqtt_get_mqtt_sub_cfg(void)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+MQTTSUB=%u,\"%.*s/"AT_MQTT_TOPIC_COMMAND"\",%u",
            AT_MQTT_LINK_ID,
            AT_MQTT_TOPIC_LEN, _at_mqtt_ctx->topic_header,
            AT_MQTT_QOS
            );
        ret->str[ret->len] = 0;
    }
    return ret;
}


at_esp_cmd_t* at_mqtt_get_mqtt_conn_cfg(void)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+MQTTCONNCFG=%u,%u,%u,\"%.*s/%s\",\"%s\",%u,%u",
            AT_MQTT_LINK_ID,
            AT_MQTT_KEEP_ALIVE,
            AT_MQTT_CLEAN_SESSION,
            AT_MQTT_TOPIC_LEN, _at_mqtt_ctx->topic_header,
            AT_MQTT_TOPIC_CONNECTION,
            AT_MQTT_LWT_MSG,
            AT_MQTT_LWT_QOS,
            AT_MQTT_LWT_RETAIN
            );
        ret->str[ret->len] = 0;
    }
    return ret;
}


bool at_mqtt_topic_match(char* topic, unsigned topic_len, char* msg, unsigned len)
{
    char* p = msg;
    char* np;
    uint8_t link_id = strtoul(p, &np, 10);
    (void)link_id;

    char osm_topic[AT_MQTT_TOPIC_LEN+1];
    snprintf(osm_topic, AT_MQTT_TOPIC_LEN+1, "%.*s/%s", (int)topic_len, topic, AT_MQTT_TOPIC_COMMAND);
    osm_topic[AT_MQTT_TOPIC_LEN] = '\0';
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
    if (curr_topic_len == strnlen(osm_topic, AT_MQTT_TOPIC_LEN) &&
        strncmp(osm_topic, curr_topic + 1, curr_topic_len) == 0)
    {
        return true;
    }
    comms_debug("MQTT topics do not match.");
    return false;
}


at_esp_cmd_t* at_mqtt_get_mqtt_conn(void)
{
    at_esp_cmd_t* ret = NULL;
    if (_at_mqtt_ctx)
    {
        ret = _at_mqtt_ctx->at_esp_ctx.cmd_line;
        ret->len = snprintf(
            ret->str,
            AT_ESP_MAX_CMD_LEN,
            "AT+MQTTCONN=%u,\"%.*s\",%"PRIu16",%u",
            AT_MQTT_LINK_ID,
            AT_POE_MQTT_ADDR_MAX_LEN, _at_mqtt_ctx->mem->addr,
            _at_mqtt_ctx->mem->port,
            AT_MQTT_RECONNECT
            );
        ret->str[ret->len] = 0;
    }
    return ret;
}


static int _at_mqtt_do_command(char* payload, unsigned payload_len, char* resp_buf, unsigned resp_buflen)
{
    command_response_t ret_code = cmds_process(payload, payload_len, NULL);
    int resp_payload_len = snprintf(
        resp_buf,
        resp_buflen,
        "{\"ret_code\":0x%"PRIX8"}",
        (uint8_t)ret_code
        );
    resp_buf[resp_payload_len] = 0;
    return resp_payload_len;
}


static int _at_mqtt_parse_payload(char* topic, unsigned topic_len, char* payload, unsigned payload_len, char* resp_buf, unsigned resp_buflen)
{
    unsigned len;
    /* Check for this sensor */
    unsigned own_topic_header_len = strnlen(_at_mqtt_ctx->topic_header, AT_MQTT_TOPIC_LEN);
    if (topic_len >= own_topic_header_len &&
        strncmp(_at_mqtt_ctx->topic_header, topic, own_topic_header_len) == 0)
    {
        char* topic_tail = topic + own_topic_header_len;
        unsigned topic_tail_len = topic_len - own_topic_header_len;
        if (topic_tail_len)
        {
            if (topic_tail[0] != '/')
            {
                /* Bad format */
                return -1;
            }
            topic_tail++;
            topic_tail_len--;
        }
        if (is_str(AT_MQTT_TOPIC_COMMAND, topic_tail, topic_tail_len))
        {
            /* Topic is command */
            len = _at_mqtt_do_command(payload, payload_len, resp_buf, resp_buflen);
        }
    }
    /* leave room for potential broadcast messages */
    comms_debug("Command from OTA");
    return len;
}


static int _at_mqtt_parse_msg(char* msg, unsigned len, char* resp_buf, unsigned resp_buflen)
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
        return AT_ERROR_CODE;
    }
    if (np[0] != ',' || np[1] != '"')
    {
        return AT_ERROR_CODE;
    }
    p = np;
    char* topic = np + 2;
    np = strchr(topic, '"');
    if (!np)
    {
        return AT_ERROR_CODE;
    }
    unsigned topic_len = np - p - 2;
    if (topic_len > len)
    {
        return AT_ERROR_CODE;
    }
    topic[topic_len] = 0;
    np = topic + topic_len + 1;
    if (np[0] != ',')
    {
        return AT_ERROR_CODE;
    }
    p = np + 1;
    unsigned payload_length = strtoul(p, &np, 10);
    if (p == np)
    {
        return AT_ERROR_CODE;
    }
    if (np[0] != ',')
    {
        return AT_ERROR_CODE;
    }
    char* payload = np + 1;
    return _at_mqtt_parse_payload(topic, topic_len, payload, payload_length, resp_buf, resp_buflen);
}


int at_mqtt_process_event(char* msg, unsigned len, char* resp_buf, unsigned resp_buflen)
{
    int r = AT_ERROR_CODE;
    const char recv_msg[] = "+MQTTSUBRECV:";
    const unsigned recv_msg_len = strlen(recv_msg);
    if (recv_msg_len <= len &&
        is_str(recv_msg, msg, recv_msg_len))
    {
        /* Received message */
        char* msg_tail = msg + recv_msg_len;
        unsigned msg_tail_len = len - recv_msg_len;
        r = _at_mqtt_parse_msg(msg_tail, msg_tail_len, resp_buf, resp_buflen);
    }
    return r;
}


bool at_mqtt_parse_mqtt_conn(char* msg, unsigned len)
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
    return conn == AT_MQTT_CONN_STATE_CONN_EST;
}


static command_response_t _at_mqtt_config_addr_cb(char* args, cmd_ctx_t * ctx)
{
    at_esp_config_get_set_str(
        "ADDR",
        _at_mqtt_ctx->mem->addr,
        AT_MQTT_ADDR_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_mqtt_config_user_cb(char* args, cmd_ctx_t * ctx)
{
    at_esp_config_get_set_str(
        "USER",
        _at_mqtt_ctx->mem->user,
        AT_MQTT_USER_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_mqtt_config_pwd_cb(char* args, cmd_ctx_t * ctx)
{
    at_esp_config_get_set_str(
        "PWD",
        _at_mqtt_ctx->mem->pwd,
        AT_MQTT_PWD_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_mqtt_config_scheme_cb(char* args, cmd_ctx_t * ctx)
{
    return at_esp_config_get_set_u16(
        "SCHEME",
        &_at_mqtt_ctx->mem->scheme,
        args, ctx) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _at_mqtt_config_ca_cb(char* args, cmd_ctx_t * ctx)
{
    at_esp_config_get_set_str(
        "CA",
        _at_mqtt_ctx->mem->ca,
        AT_POE_MQTT_CA_MAX_LEN,
        args, ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _at_mqtt_config_port_cb(char* args, cmd_ctx_t * ctx)
{
    return at_esp_config_get_set_u16(
        "PORT",
        &_at_mqtt_ctx->mem->port,
        args, ctx) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


struct cmd_link_t* at_mqtt_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "mqtt_addr",      "Set/get MQTT address",     _at_mqtt_config_addr_cb        , false , NULL },
        { "mqtt_user",      "Set/get MQTT user",        _at_mqtt_config_user_cb        , false , NULL },
        { "mqtt_pwd",       "Set/get MQTT password",    _at_mqtt_config_pwd_cb         , false , NULL },
        { "mqtt_sch",       "Set/get MQTT scheme",      _at_mqtt_config_scheme_cb      , false , NULL },
        { "mqtt_ca",        "Set/get MQTT CA",          _at_mqtt_config_ca_cb          , false , NULL },
        { "mqtt_port",      "Set/get MQTT port",        _at_mqtt_config_port_cb        , false , NULL }
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}


bool at_mqtt_get_id(char* str, uint8_t len)
{
    strncpy(str, _at_mqtt_ctx->topic_header, len);
    return true;
}


void at_mqtt_cmd_j_cfg(cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_ADDR(_at_mqtt_ctx->mem->addr));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_USER(_at_mqtt_ctx->mem->user));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_PWD(_at_mqtt_ctx->mem->pwd));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_SCHEME(_at_mqtt_ctx->mem->scheme));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_CA(_at_mqtt_ctx->mem->ca));
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,AT_MQTT_PRINT_CFG_JSON_MQTT_PORT(_at_mqtt_ctx->mem->port));
    cmd_ctx_flush(ctx);
}


void at_mqtt_config_init(at_mqtt_config_t* conf)
{
    memset(conf, 0, sizeof(at_poe_config_t));
    conf->scheme = AT_MQTT_SCHEME_WSS_NO_CERT;
    conf->port = 443;
}
