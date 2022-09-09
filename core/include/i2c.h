#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef enum
{
    I2C_TYPE_HTU21D,
    I2C_TYPE_VEML7700,
} i2c_type_t;


extern void i2cs_init(void);

extern bool i2c_transfer_timeout(i2c_type_t type, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms);

