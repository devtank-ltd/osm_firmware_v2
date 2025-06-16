#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>

void        osm_protocol_system_init(void);

bool        osm_protocol_init(void);
bool        osm_protocol_append_measurement(measurements_def_t* def, measurements_data_t* data);
bool        osm_protocol_send(void);
void        osm_protocol_send_error_code(uint8_t err_code);

void        osm_protocol_loop_iteration(void);

bool        osm_protocol_send_ready(void);
bool        osm_protocol_send_allowed(void);
void        osm_protocol_reset(void);
void        osm_protocol_process(char* message);
bool        osm_protocol_get_connected(void);
void        osm_protocol_loop_iteration(void);

bool        osm_protocol_get_id(char* str, uint8_t len);

struct cmd_link_t* osm_protocol_add_commands(struct cmd_link_t* tail);

void        osm_protocol_power_down(void);

/* To be implemented by caller.*/
void     osm_on_protocol_sent_ack(bool acked) __attribute__((weak));
