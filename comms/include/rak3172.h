#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "base_types.h"


extern uint16_t rak3172_get_mtu(void);

extern bool     rak3172_send_ready(void);
extern bool     rak3172_send_str(char* str);
extern bool     rak3172_send_allowed(void);
extern void     rak3172_send(int8_t* hex_arr, uint16_t arr_len);

extern void     rak3172_init(void);
extern void     rak3172_reset(void);
extern void     rak3172_process(char* message);
extern bool     rak3172_get_connected(void);
extern void     rak3172_loop_iteration(void);

extern command_response_t rak3172_config_setup_str(char * str);

extern bool     rak3172_get_id(char* str, uint8_t len);

extern struct cmd_link_t* rak3172_add_commands(struct cmd_link_t* tail);

extern void     rak3172_power_down(void);
