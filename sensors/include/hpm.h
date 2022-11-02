#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "ring.h"
#include "measurements.h"

extern void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

extern void hpm_pm10_inf_init(measurements_inf_t* inf);
extern void hpm_pm25_inf_init(measurements_inf_t* inf);
