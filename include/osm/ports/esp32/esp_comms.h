#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <osm/core/base_types.h>

extern bool        esp_comms_send_ready(void);
extern bool        esp_comms_send_allowed(void);
extern void        esp_comms_reset(void);

extern bool        esp_comms_send(char * data, uint16_t len);

extern void        esp_comms_init(void);

extern bool        esp_comms_get_connected(void);

extern void        esp_comms_loop_iteration(void);

extern void        esp_comms_power_down(void);

extern bool        esp_comms_get_unix_time(int64_t * ts);

extern void        esp_comms_process(char* message);
extern bool        esp_comms_send_str(char* str);
extern bool        esp_comms_get_id(char* str, uint8_t len);
