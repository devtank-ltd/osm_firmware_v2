#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

bool        osm_esp_comms_send_ready(void);
bool        osm_esp_comms_send_allowed(void);
void        osm_esp_comms_reset(void);

bool        osm_esp_comms_send(char * data, uint16_t len);

void        osm_esp_comms_init(void);

bool        osm_esp_comms_get_connected(void);

void        osm_esp_comms_loop_iteration(void);

void        osm_esp_comms_power_down(void);

bool        osm_esp_comms_get_unix_time(int64_t * ts);

void        osm_esp_comms_process(char* message);
bool        osm_esp_comms_send_str(char* str);
bool        osm_esp_comms_get_id(char* str, uint8_t len);
