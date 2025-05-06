#pragma once

#include <stdint.h>
#include <stdbool.h>


#include <osm/core/ring.h>
#include <osm/core/measurements.h>

void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

void hpm_pm10_inf_init(measurements_inf_t* inf);
void hpm_pm25_inf_init(measurements_inf_t* inf);
