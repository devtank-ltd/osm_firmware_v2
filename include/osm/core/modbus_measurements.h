#pragma once

#include <osm/core/measurements.h>
#include <osm/core/modbus.h>

void modbus_inf_init(measurements_inf_t* inf);

bool modbus_measurement_add(modbus_reg_t * reg);

bool modbus_measurement_del_reg(char * name);
bool modbus_measurement_del_dev(char * name);
