#pragma once

#include "measurements.h"
#include "modbus.h"

extern void modbus_inf_init(measurements_inf_t* inf);

extern bool modbus_measurement_add(modbus_reg_t * reg);

extern void modbus_measurement_del_reg(char * name);
extern void modbus_measurement_del_dev(char * name);
