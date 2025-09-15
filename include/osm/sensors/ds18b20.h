#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void                         osm_ds18b20_enable(bool enable);

void                         osm_ds18b20_temp_init(void);

void                         osm_ds18b20_inf_init(osm_measurements_inf_t* inf);
