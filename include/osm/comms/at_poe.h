#pragma once

#include <inttypes.h>
#include <stdbool.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/comms/at_mqtt.h>

#ifndef comms_name
#define comms_name          at_poe
#endif //comms_name

#define OSM_COMMS_BUILD_TYPE    OSM_COMMS_TYPE_POE

typedef struct
{
    uint8_t     type;
    uint8_t     _[15];
    /* 16 byte boundary ---- */
    osm_at_mqtt_config_t mqtt;
    /* 16 byte boundary ---- */
} __attribute__((__packed__)) osm_at_poe_config_t;

OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_at_poe_config_t, mqtt);

_Static_assert(sizeof(osm_at_poe_config_t) <= sizeof(osm_comms_config_t), "COMMS config too big.");


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

struct osm_cmd_link_t* osm_at_poe_add_commands(struct osm_cmd_link_t* tail);

void     osm_at_poe_power_down(void);

bool     osm_at_poe_persist_config_cmp(void* d0, void* d1);
void     osm_at_poe_config_init(osm_comms_config_t* comms_config);

osm_command_response_t osm_at_poe_cmd_config_cb(char * args, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_at_poe_cmd_j_cfg_cb(char* args, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_at_poe_cmd_conn_cb(char* args, osm_cmd_ctx_t * ctx);

bool osm_at_poe_get_unix_time(int64_t * ts);
