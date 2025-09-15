#pragma once

#include <osm/core/measurements.h>
#include <osm/core/modbus.h>

void osm_modbus_inf_init(osm_measurements_inf_t* inf);

bool osm_modbus_measurement_add(osm_modbus_reg_t * reg);

bool osm_modbus_measurement_del_reg(char * name);
bool osm_modbus_measurement_del_dev(char * name);
