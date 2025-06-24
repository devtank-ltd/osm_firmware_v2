#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void osm_veml7700_init(void);

void osm_veml7700_inf_init(osm_measurements_inf_t* inf);
