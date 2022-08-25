#pragma once

#include <stdbool.h>
#include <stdint.h>

extern void veml7700_init(void);

extern bool veml7700_get_lux(uint32_t* lux);


extern measurements_sensor_state_t  veml7700_measurements_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  veml7700_light_measurements_init(char* name);
extern measurements_sensor_state_t  veml7700_light_measurements_get(char* name, measurements_reading_t* value);
extern measurements_sensor_state_t  veml7700_iteration(char* name);
