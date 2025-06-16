#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void osm_htu21d_init(void);

void osm_htu21d_temp_inf_init(measurements_inf_t* inf);
void osm_htu21d_humi_inf_init(measurements_inf_t* inf);
