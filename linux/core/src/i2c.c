#include <stdbool.h>
#include <stdint.h>

#include "i2c.h"


void i2c_init(unsigned i2c_index)
{
    return;
}

bool i2c_transfer_timeout(uint32_t i2c, uint8_t addr, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms)
{
    return false;
}
