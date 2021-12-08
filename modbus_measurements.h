#pragma once

#include "measurements.h"

extern uint32_t modbus_bus_collection_time(void);
extern bool modbus_init(char* name);
extern bool modbus_get(char* name, value_t* value);
