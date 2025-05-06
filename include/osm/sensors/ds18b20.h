#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void                         ds18b20_enable(bool enable);

void                         ds18b20_temp_init(void);

void                         ds18b20_inf_init(measurements_inf_t* inf);
