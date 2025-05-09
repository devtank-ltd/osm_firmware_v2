#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/comms/at_mqtt.h>

#ifndef comms_name
#define comms_name          at_wifi
#endif //comms_name

#define COMMS_BUILD_TYPE    COMMS_TYPE_WIFI

#define AT_WIFI_MAX_SSID_LEN                    63
#define AT_WIFI_MAX_PWD_LEN                     63
#define AT_WIFI_MAX_COUNTRY_CODE_LEN            2

typedef struct
{
    uint8_t     type;
    uint16_t    channel_start;
    uint16_t    channel_count;
    uint8_t     _[11];
    /* 16 byte boundary ---- */
    at_mqtt_config_t mqtt;
    /* 16 byte boundary ---- */
    struct
    {
        char        ssid[AT_WIFI_MAX_SSID_LEN + 1];
        /* 16 byte boundary ---- */
        char        pwd[AT_WIFI_MAX_PWD_LEN + 1];
    } wifi;
    /* 16 byte boundary ---- */
    char        country_code[AT_WIFI_MAX_COUNTRY_CODE_LEN + 1];
    uint8_t     __[13];
    /* 16 byte boundary ---- */
} __attribute__((__packed__)) at_wifi_config_t;

_Static_assert(sizeof(at_wifi_config_t) <= sizeof(comms_config_t), "COMMS config too big.");


uint16_t at_wifi_get_mtu(void);

bool     at_wifi_send_ready(void);
bool     at_wifi_send_str(char* str);
bool     at_wifi_send_allowed(void);
bool     at_wifi_send(char* data, uint16_t len);

void     at_wifi_init(void);
void     at_wifi_reset(void);
void     at_wifi_process(char* message);
bool     at_wifi_get_connected(void);
void     at_wifi_loop_iteration(void);

bool     at_wifi_get_id(char* str, uint8_t len);

struct cmd_link_t* at_wifi_add_commands(struct cmd_link_t* tail);

void     at_wifi_power_down(void);

bool     at_wifi_persist_config_cmp(void* d0, void* d1);
void     at_wifi_config_init(comms_config_t* comms_config);

command_response_t at_wifi_cmd_config_cb(char * args, cmd_ctx_t * ctx);
command_response_t at_wifi_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx);
command_response_t at_wifi_cmd_conn_cb(char* args, cmd_ctx_t * ctx);

bool at_wifi_get_unix_time(int64_t * ts);
