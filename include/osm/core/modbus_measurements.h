#pragma once

#include <osm/core/measurements.h>
#include <osm/core/modbus.h>

extern void modbus_inf_init(measurements_inf_t* inf);

extern bool modbus_measurement_add(modbus_reg_t * reg);

extern bool modbus_measurement_del_reg(char * name);
extern bool modbus_measurement_del_dev(char * name);
