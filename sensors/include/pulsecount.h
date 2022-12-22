#pragma once
#include <stdbool.h>

#include "base_types.h"
#include "measurements.h"


extern void     pulsecount_init(void);

extern void     pulsecount_enable(unsigned io, bool enable, bool hw_pup);

extern void     pulsecount_inf_init(measurements_inf_t* inf);
