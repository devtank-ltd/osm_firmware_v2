#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "measurements.h"


extern void                         ds18b20_enable(bool enable);

extern bool                         ds18b20_query_temp(float* temperature, char* name);

extern measurements_sensor_state_t  ds18b20_measurements_init(char* name, bool in_isolation);
extern measurements_sensor_state_t  ds18b20_measurements_collect(char* name, measurements_reading_t* value);
extern measurements_sensor_state_t  ds18b20_collection_time(char* name, uint32_t* collection_time);

extern void                         ds18b20_temp_init(void);
