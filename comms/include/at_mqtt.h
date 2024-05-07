#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include "at_base.h"

#define COMMS_DEFAULT_MTU                                   (1024 + 128)

#define AT_MQTT_ADDR_MAX_LEN                                63
#define AT_MQTT_USER_MAX_LEN                                63
#define AT_MQTT_PWD_MAX_LEN                                 63
#define AT_MQTT_CA_MAX_LEN                                  63
#define AT_MQTT_TOPIC_LEN                                   63
#define AT_MQTT_RESP_PAYLOAD_LEN                            63

#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_ADDR                "    \"MQTT ADDR\": \"%.*s\","
#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_USER                "    \"MQTT USER\": \"%.*s\","
#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_PWD                 "    \"MQTT PWD\": \"%.*s\","
#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_SCHEME              "    \"MQTT SCHEME\": %"PRIu16","
#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_CA                  "    \"MQTT CA\": \"%.*s\","
#define AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_PORT                "    \"MQTT PORT\": %"PRIu16""

#define AT_MQTT_PRINT_CFG_JSON_MQTT_ADDR(_mqtt_addr)        AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_ADDR    , AT_MQTT_ADDR_MAX_LEN , _mqtt_addr
#define AT_MQTT_PRINT_CFG_JSON_MQTT_USER(_mqtt_user)        AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_USER    , AT_MQTT_USER_MAX_LEN , _mqtt_user
#define AT_MQTT_PRINT_CFG_JSON_MQTT_PWD(_mqtt_pwd)          AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_PWD     , AT_MQTT_PWD_MAX_LEN  , _mqtt_pwd
#define AT_MQTT_PRINT_CFG_JSON_MQTT_SCHEME(_mqtt_scheme)    AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_SCHEME  , _mqtt_scheme
#define AT_MQTT_PRINT_CFG_JSON_MQTT_CA(_mqtt_ca)            AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_CA      , AT_MQTT_CA_MAX_LEN   , _mqtt_ca
#define AT_MQTT_PRINT_CFG_JSON_MQTT_PORT(_mqtt_port)        AT_MQTT_PRINT_CFG_JSON_FMT_MQTT_PORT    , _mqtt_port

#define AT_MQTT_TOPIC_MEASUREMENTS                          "measurements"
#define AT_MQTT_TOPIC_COMMAND                               "cmd"
#define AT_MQTT_TOPIC_CONNECTION                            "conn"
#define AT_MQTT_TOPIC_COMMAND_RESP                          AT_MQTT_TOPIC_COMMAND"/resp"

#define AT_ERROR_CODE                                       -1


enum at_mqtt_scheme_t
{
    AT_MQTT_SCHEME_BARE                                = 1,  /* MQTT over TCP. */
    AT_MQTT_SCHEME_TLS_NO_CERT                         = 2,  /* MQTT over TLS (no certificate verify). */
    AT_MQTT_SCHEME_TLS_VERIFY_SERVER                   = 3,  /* MQTT over TLS (verify server certificate). */
    AT_MQTT_SCHEME_TLS_PROVIDE_CLIENT                  = 4,  /* MQTT over TLS (provide client certificate). */
    AT_MQTT_SCHEME_TLS_VERIFY_SERVER_PROVIDE_CLIENT    = 5,  /* MQTT over TLS (verify server certificate and provide client certificate). */
    AT_MQTT_SCHEME_WS                                  = 6,  /* MQTT over Websocket (TCP) */
    AT_MQTT_SCHEME_WSS_NO_CERT                         = 7,  /* MQTT over Websocket Secure (TLS no certificate verify). */
    AT_MQTT_SCHEME_WSS_VERIFY_SERVER                   = 8,  /* MQTT over Websocket Secure (TLS verify server certificate). */
    AT_MQTT_SCHEME_WSS_PROVIDE_CLIENT                  = 9,  /* MQTT over Websocket Secure (TLS provide client certificate). */
    AT_MQTT_SCHEME_WSS_VERIFY_SERVER_PROVIDE_CLIENT    = 10, /* MQTT over Websocket Secure (TLS verify server certificate and provide client certificate). */
    AT_MQTT_SCHEME_COUNT,
};


enum at_mqtt_conn_states_t
{
    AT_MQTT_CONN_STATE_NOT_INIT             = 0,
    AT_MQTT_CONN_STATE_SET_USR_CFG          = 1,
    AT_MQTT_CONN_STATE_SET_CONN_CFG         = 2,
    AT_MQTT_CONN_STATE_DISCONNECTED         = 3,
    AT_MQTT_CONN_STATE_CONN_EST             = 4,
    AT_MQTT_CONN_STATE_CONN_EST_NO_TOPIC    = 5,
    AT_MQTT_CONN_STATE_CONN_EST_WITH_TOPIC  = 6,
};


typedef struct
{
    char        addr[AT_MQTT_ADDR_MAX_LEN + 1];
    char        user[AT_MQTT_USER_MAX_LEN + 1];
    char        pwd[AT_MQTT_PWD_MAX_LEN + 1];
    char        ca[AT_MQTT_CA_MAX_LEN + 1];
    uint16_t    scheme; /* at_wifi_mqtt_scheme_t */
    uint16_t    port;
} at_mqtt_config_t;


typedef struct
{
    at_base_ctx_t        at_base_ctx;
    at_mqtt_config_t*   mem;
    char                topic_header[AT_MQTT_TOPIC_LEN + 1];
    struct
    {
        char message[COMMS_DEFAULT_MTU];
        unsigned len;
    } publish_packet;
} at_mqtt_ctx_t;




bool                at_mqtt_mem_is_valid(void);
at_base_cmd_t*       at_mqtt_publish_prep(const char* topic, char* message, unsigned message_len);
void                at_mqtt_init(at_mqtt_ctx_t* ctx);
at_base_cmd_t*       at_mqtt_get_ntp_cfg(void);
at_base_cmd_t*       at_mqtt_get_mqtt_user_cfg(void);
at_base_cmd_t*       at_mqtt_get_mqtt_sub_cfg(void);
at_base_cmd_t*       at_mqtt_get_mqtt_conn_cfg(void);
bool                at_mqtt_topic_match(char* topic, unsigned topic_len, char* msg, unsigned len);
at_base_cmd_t*       at_mqtt_get_mqtt_conn(void);
int                 at_mqtt_process_event(char* msg, unsigned len, char* resp_buf, unsigned resp_buflen);
bool                at_mqtt_parse_mqtt_conn(char* msg, unsigned len);
struct cmd_link_t*  at_mqtt_add_commands(struct cmd_link_t* tail);
bool                at_mqtt_get_id(char* str, uint8_t len);
void                at_mqtt_cmd_j_cfg(cmd_ctx_t * ctx);
void                at_mqtt_config_init(at_mqtt_config_t* conf);
