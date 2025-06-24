#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

#ifndef comms_name
#define comms_name          rak4270
#endif //comms_name

#define OSM_COMMS_BUILD_TYPE    OSM_COMMS_TYPE_LW

uint16_t osm_rak4270_get_mtu(void);

bool     osm_rak4270_send_ready(void);
bool     osm_rak4270_send_str(char* str);
bool     osm_rak4270_send_allowed(void);
bool     osm_rak4270_send(int8_t* hex_arr, uint16_t arr_len);

void     osm_rak4270_init(void);
void     osm_rak4270_reset(void);
void     osm_rak4270_process(char* message);
bool     osm_rak4270_get_connected(void);
void     osm_rak4270_loop_iteration(void);

bool     osm_rak4270_get_id(char* str, uint8_t len);

struct osm_cmd_link_t* osm_rak4270_add_commands(struct osm_cmd_link_t* tail);

void     osm_rak4270_power_down(void);

bool     osm_rak4270_persist_config_cmp(void* d0, void* d1);

osm_command_response_t osm_rak4270_cmd_config_cb(char* str, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_rak4270_cmd_j_cfg_cb(char* str, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_rak4270_cmd_conn_cb(char* str, osm_cmd_ctx_t * ctx);
