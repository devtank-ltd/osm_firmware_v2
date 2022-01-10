#pragma once

#include <stdbool.h>
#include <stdint.h>

extern void htu21d_init(void);

extern bool htu21d_read_temp(int32_t* temp);

extern bool htu21d_read_humidity(int32_t* humidity);

extern bool htu21d_read_dew_temp(int32_t* dew_temp);
