#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

#ifndef comms_name
#define comms_name          linux_comms
#define OSM_COMMS_BUILD_TYPE    OSM_COMMS_TYPE_LW
#endif //comms_name

uint16_t osm_linux_comms_get_mtu(void);

bool     osm_linux_comms_send_ready(void);
bool     osm_linux_comms_send_str(char* str);
bool     osm_linux_comms_send_allowed(void);
bool     osm_linux_comms_send(int8_t* hex_arr, uint16_t arr_len);

void     osm_linux_comms_config_init(osm_comms_config_t* config);
bool     osm_linux_comms_persist_config_cmp(osm_comms_config_t* d0, osm_comms_config_t* d1);

void     osm_linux_comms_init(void);
void     osm_linux_comms_reset(void);
void     osm_linux_comms_process(char* message);
bool     osm_linux_comms_get_connected(void);
void     osm_linux_comms_loop_iteration(void);

void     osm_linux_comms_config_setup_str(char * str, osm_cmd_ctx_t * ctx);

bool     osm_linux_comms_get_id(char* str, uint8_t len);

struct osm_cmd_link_t* osm_linux_comms_add_commands(struct osm_cmd_link_t* tail);

void     osm_linux_comms_power_down(void);

osm_command_response_t osm_linux_comms_cmd_config_cb(char * args, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_linux_comms_cmd_j_cfg_cb(char* args, osm_cmd_ctx_t * ctx);
osm_command_response_t osm_linux_comms_cmd_conn_cb(char* args, osm_cmd_ctx_t * ctx);
