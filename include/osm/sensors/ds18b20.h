#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "measurements.h"


extern void                         ds18b20_enable(bool enable);

extern void                         ds18b20_temp_init(void);

extern void                         ds18b20_inf_init(measurements_inf_t* inf);
