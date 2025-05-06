#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void veml7700_init(void);

void veml7700_inf_init(measurements_inf_t* inf);
