#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


extern void veml7700_init(void);

extern void veml7700_inf_init(measurements_inf_t* inf);
