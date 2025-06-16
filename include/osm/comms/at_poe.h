#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/comms/at_mqtt.h>

#ifndef comms_name
#define comms_name          at_poe
#endif //comms_name

#define COMMS_BUILD_TYPE    COMMS_TYPE_POE

typedef struct
{
    uint8_t     type;
    uint8_t     _[15];
    /* 16 byte boundary ---- */
    at_mqtt_config_t mqtt;
    /* 16 byte boundary ---- */
} __attribute__((__packed__)) at_poe_config_t;

STATIC_ASSERT_16BYTE_ALIGNED(at_poe_config_t, mqtt);

_Static_assert(sizeof(at_poe_config_t) <= sizeof(comms_config_t), "COMMS config too big.");


uint16_t osm_at_poe_get_mtu(void);

bool     osm_at_poe_send_ready(void);
bool     osm_at_poe_send_str(char* str);
bool     osm_at_poe_send_allowed(void);
bool     osm_at_poe_send(char* data, uint16_t len);

void     osm_at_poe_init(void);
void     osm_at_poe_reset(void);
void     osm_at_poe_process(char* message);
bool     osm_at_poe_get_connected(void);
void     osm_at_poe_loop_iteration(void);

bool     osm_at_poe_get_id(char* str, uint8_t len);

struct cmd_link_t* osm_at_poe_add_commands(struct cmd_link_t* tail);

void     osm_at_poe_power_down(void);

bool     osm_at_poe_persist_config_cmp(void* d0, void* d1);
void     osm_at_poe_config_init(comms_config_t* comms_config);

command_response_t osm_at_poe_cmd_config_cb(char * args, cmd_ctx_t * ctx);
command_response_t osm_at_poe_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx);
command_response_t osm_at_poe_cmd_conn_cb(char* args, cmd_ctx_t * ctx);

bool osm_at_poe_get_unix_time(int64_t * ts);
