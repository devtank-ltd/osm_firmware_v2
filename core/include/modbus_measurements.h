#pragma once

#include "measurements.h"
#include "modbus.h"

extern measurements_sensor_state_t modbus_measurements_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t modbus_measurements_init(char* name);
extern measurements_sensor_state_t modbus_measurements_init2(modbus_reg_t * reg);
extern measurements_sensor_state_t modbus_measurements_get(char* name, value_t* value);
extern measurements_sensor_state_t modbus_measurements_get2(modbus_reg_t * reg, value_t* value);

extern bool modbus_measurement_add(modbus_reg_t * reg);

extern void modbus_measurement_del_reg(char * name);
extern void modbus_measurement_del_dev(char * name);
