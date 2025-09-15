#pragma once

#include <stdbool.h>
#include <stdint.h>


void osm_i2cs_init(void);
void osm_i2cs_deinit(void);
bool osm_i2c_transfer_timeout(uint32_t i2c, uint8_t addr, const uint8_t *w, unsigned wn, uint8_t *r, unsigned rn, unsigned timeout_ms);

