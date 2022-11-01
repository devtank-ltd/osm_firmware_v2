#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"


extern void veml7700_init(void);

extern measurements_sensor_state_t  veml7700_measurements_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  veml7700_light_measurements_init(char* name, bool in_isolation);
extern measurements_sensor_state_t  veml7700_light_measurements_get(char* name, measurements_reading_t* value);
extern measurements_sensor_state_t  veml7700_iteration(char* name);
