#pragma once

#include <stdbool.h>
#include <stdint.h>

extern void htu21d_init(void);

extern bool htu21d_read_temp(uint16_t * temp);

extern bool htu21d_read_humidity(uint16_t * humidity);
