#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/measurements.h>


void htu21d_init(void);

void htu21d_temp_inf_init(measurements_inf_t* inf);
void htu21d_humi_inf_init(measurements_inf_t* inf);
