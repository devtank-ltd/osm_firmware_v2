#pragma once


#include "measurements.h"


extern measurements_sensor_state_t  bat_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  bat_begin(char* name, bool in_isolation);
extern measurements_sensor_state_t  bat_get(char* name, measurements_reading_t* value);

extern bool                         bat_get_blocking(char* name, measurements_reading_t* value);
extern bool                         bat_on_battery(bool* on_battery);
