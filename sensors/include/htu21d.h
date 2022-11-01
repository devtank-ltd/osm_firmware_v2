#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "measurements.h"


extern void htu21d_init(void);

extern measurements_sensor_state_t htu21d_measurements_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t htu21d_measurements_iteration(char* name);

extern measurements_sensor_state_t htu21d_temp_measurements_init(char* name, bool in_isolation);
extern measurements_sensor_state_t htu21d_temp_measurements_get(char* name, measurements_reading_t* value);

extern measurements_sensor_state_t htu21d_humi_measurements_init(char* name, bool in_isolation);
extern measurements_sensor_state_t htu21d_humi_measurements_get(char* name, measurements_reading_t* value);
