#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "base_types.h"

#ifndef comms_name
#define comms_name          rak4270
#endif //comms_name

#define COMMS_BUILD_TYPE    COMMS_TYPE_LW

extern uint16_t rak4270_get_mtu(void);

extern bool     rak4270_send_ready(void);
extern bool     rak4270_send_str(char* str);
extern bool     rak4270_send_allowed(void);
extern bool     rak4270_send(int8_t* hex_arr, uint16_t arr_len);

extern void     rak4270_init(void);
extern void     rak4270_reset(void);
extern void     rak4270_process(char* message);
extern bool     rak4270_get_connected(void);
extern void     rak4270_loop_iteration(void);

extern bool     rak4270_get_id(char* str, uint8_t len);

extern struct cmd_link_t* rak4270_add_commands(struct cmd_link_t* tail);

extern void     rak4270_power_down(void);

extern bool     rak4270_persist_config_cmp(void* d0, void* d1);

extern command_response_t rak4270_cmd_config_cb(char* str, cmd_ctx_t * ctx);
extern command_response_t rak4270_cmd_j_cfg_cb(char* str, cmd_ctx_t * ctx);
extern command_response_t rak4270_cmd_conn_cb(char* str, cmd_ctx_t * ctx);
