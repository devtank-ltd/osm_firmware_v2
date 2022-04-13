#pragma once


#include "measurements.h"


extern measurements_sensor_state_t  adcs_bat_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  adcs_bat_begin(char* name);
extern measurements_sensor_state_t  adcs_bat_get(char* name, value_t* value);

extern bool                         adcs_bat_get_blocking(char* name, value_t* value);
