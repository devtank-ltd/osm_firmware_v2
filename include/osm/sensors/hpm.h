#pragma once

#include <stdint.h>
#include <stdbool.h>


#include <osm/core/ring.h>
#include <osm/core/measurements.h>

void osm_hpm_ring_process(osm_ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

void osm_hpm_pm10_inf_init(osm_measurements_inf_t* inf);
void osm_hpm_pm25_inf_init(osm_measurements_inf_t* inf);
