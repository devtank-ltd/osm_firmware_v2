#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifndef comms_name
#define comms_name          linux_comms
#endif //comms_name

extern uint16_t linux_comms_get_mtu(void);

extern bool     linux_comms_send_ready(void);
extern bool     linux_comms_send_str(char* str);
extern bool     linux_comms_send_allowed(void);
extern void     linux_comms_send(int8_t* hex_arr, uint16_t arr_len);

extern void     linux_comms_init(void);
extern void     linux_comms_reset(void);
extern void     linux_comms_process(char* message);
extern bool     linux_comms_get_connected(void);
extern void     linux_comms_loop_iteration(void);

extern void     linux_comms_config_setup_str(char * str);

extern bool     linux_comms_get_id(char* str, uint8_t len);

extern struct cmd_link_t* linux_comms_add_commands(struct cmd_link_t* tail);

extern void     linux_comms_power_down(void);
