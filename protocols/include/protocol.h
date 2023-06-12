#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"
#include "measurements.h"

void        protocol_system_init(void);

bool        protocol_init(void);
bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data);
bool        protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type);
void        protocol_debug(void);
void        protocol_send(void);
void        protocol_send_error_code(uint8_t err_code);

void        protocol_loop_iteration(void);

bool        protocol_send_ready(void);
bool        protocol_send_allowed(void);
void        protocol_reset(void);
void        protocol_process(char* message);
bool        protocol_get_connected(void);
void        protocol_loop_iteration(void);

bool        protocol_get_id(char* str, uint8_t len);

struct cmd_link_t* protocol_add_commands(struct cmd_link_t* tail);

void        protocol_power_down(void);

/* To be implemented by caller.*/
extern void     on_protocol_sent_ack(bool acked) __attribute__((weak));

