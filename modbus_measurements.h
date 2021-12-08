#pragma once

#include "measurements.h"

extern uint32_t modbus_measurements_collection_time(void);
extern bool modbus_measurements_init(char* name);
extern bool modbus_measurements_get(char* name, value_t* value);
