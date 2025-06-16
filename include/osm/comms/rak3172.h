#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

#ifndef comms_name
#define comms_name          rak3172
#endif //comms_name

#define OSM_COMMS_BUILD_TYPE    COMMS_TYPE_LW

uint16_t osm_rak3172_get_mtu(void);

bool     osm_rak3172_send_ready(void);
bool     osm_rak3172_send_str(char* str);
bool     osm_rak3172_send_allowed(void);
bool     osm_rak3172_send(int8_t* hex_arr, uint16_t arr_len);

void     osm_rak3172_init(void);
void     osm_rak3172_reset(void);
void     osm_rak3172_process(char* message);
bool     osm_rak3172_get_connected(void);
void     osm_rak3172_loop_iteration(void);

bool     osm_rak3172_get_id(char* str, uint8_t len);

struct cmd_link_t* osm_rak3172_add_commands(struct cmd_link_t* tail);

void     osm_rak3172_power_down(void);

bool     osm_rak3172_persist_config_cmp(void* d0, void* d1);

command_response_t osm_rak3172_cmd_config_cb(char* str, cmd_ctx_t * ctx);
command_response_t osm_rak3172_cmd_j_cfg_cb(char* str, cmd_ctx_t * ctx);
command_response_t osm_rak3172_cmd_conn_cb(char* str, cmd_ctx_t * ctx);
