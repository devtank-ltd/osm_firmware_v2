#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"
#include "measurements.h"


bool        protocol_init(int8_t* arr, unsigned len);
bool        protocol_append_measurement(measurements_def_t* def, measurements_data_t* data);
bool        protocol_append_error_code(uint8_t err_code);
bool        protocol_append_instant_measurement(measurements_def_t* def, measurements_reading_t* reading, measurements_value_type_t type);
unsigned    protocol_get_length(void);
