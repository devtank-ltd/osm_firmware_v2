#pragma once

#include "measurements.h"
#include "modbus.h"

extern uint32_t modbus_measurements_collection_time(void);
extern bool modbus_measurements_init(char* name);
extern bool modbus_measurements_get(char* name, value_t* value);

extern bool modbus_measurement_add(modbus_reg_t * reg);

extern void modbus_measurement_del_reg(char * name);
extern void modbus_measurement_del_dev(char * name);
