#pragma once


#include "measurements.h"


extern bool                         bat_get_blocking(char* name, measurements_reading_t* value);
extern bool                         bat_on_battery(bool* on_battery);

extern void                         bat_inf_init(measurements_inf_t* inf);
