# pragma once

#include "measurements.h"
#include "ring.h"


void rs232_process(ring_buf_t* ring);
void rs232_inf_init(measurements_inf_t* inf);
