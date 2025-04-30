#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

#ifndef comms_name
#define comms_name          rak4270
#endif //comms_name

#define COMMS_BUILD_TYPE    COMMS_TYPE_LW

uint16_t rak4270_get_mtu(void);

bool     rak4270_send_ready(void);
bool     rak4270_send_str(char* str);
bool     rak4270_send_allowed(void);
bool     rak4270_send(int8_t* hex_arr, uint16_t arr_len);

void     rak4270_init(void);
void     rak4270_reset(void);
void     rak4270_process(char* message);
bool     rak4270_get_connected(void);
void     rak4270_loop_iteration(void);

bool     rak4270_get_id(char* str, uint8_t len);

struct cmd_link_t* rak4270_add_commands(struct cmd_link_t* tail);

void     rak4270_power_down(void);

bool     rak4270_persist_config_cmp(void* d0, void* d1);

command_response_t rak4270_cmd_config_cb(char* str, cmd_ctx_t * ctx);
command_response_t rak4270_cmd_j_cfg_cb(char* str, cmd_ctx_t * ctx);
command_response_t rak4270_cmd_conn_cb(char* str, cmd_ctx_t * ctx);
