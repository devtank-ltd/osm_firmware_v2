#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"
#include "measurements.h"


bool        protocol_init(int8_t* arr, unsigned len);
bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data);
unsigned    protocol_get_length(void);
