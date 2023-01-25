#pragma once

#include <stdint.h>
#include <stdbool.h>


extern uint16_t comms_get_mtu(void);

extern bool     comms_send_ready(void);
extern bool     comms_send_str(char* str);
extern bool     comms_send_allowed(void);
extern void     comms_send(int8_t* hex_arr, uint16_t arr_len);

extern void     comms_init(void);
extern void     comms_reset(void);
extern void     comms_process(char* message);
extern bool     comms_get_connected(void);
extern void     comms_loop_iteration(void);

extern void     comms_config_setup_str(char * str);

extern bool     comms_get_id(char* str, uint8_t len);

/* To be implemented by caller.*/
extern void     on_comms_sent_ack(bool acked) __attribute__((weak));

extern struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail);

extern void     comms_power_down(void);
