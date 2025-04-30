#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

#ifndef comms_name
#define comms_name          rak3172
#endif //comms_name

#define COMMS_BUILD_TYPE    COMMS_TYPE_LW

uint16_t rak3172_get_mtu(void);

bool     rak3172_send_ready(void);
bool     rak3172_send_str(char* str);
bool     rak3172_send_allowed(void);
bool     rak3172_send(int8_t* hex_arr, uint16_t arr_len);

void     rak3172_init(void);
void     rak3172_reset(void);
void     rak3172_process(char* message);
bool     rak3172_get_connected(void);
void     rak3172_loop_iteration(void);

bool     rak3172_get_id(char* str, uint8_t len);

struct cmd_link_t* rak3172_add_commands(struct cmd_link_t* tail);

void     rak3172_power_down(void);

bool     rak3172_persist_config_cmp(void* d0, void* d1);

command_response_t rak3172_cmd_config_cb(char* str, cmd_ctx_t * ctx);
command_response_t rak3172_cmd_j_cfg_cb(char* str, cmd_ctx_t * ctx);
command_response_t rak3172_cmd_conn_cb(char* str, cmd_ctx_t * ctx);
