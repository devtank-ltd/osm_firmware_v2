#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "measurements.h"


extern void                         w1_enable(bool enable);

extern bool                         w1_query_temp(float* temperature);

extern measurements_sensor_state_t  w1_measurement_init(char* name);
extern measurements_sensor_state_t  w1_measurement_collect(char* name, value_t* value);
extern uint32_t                     w1_collection_time(void);

extern void                         w1_temp_init(void);
