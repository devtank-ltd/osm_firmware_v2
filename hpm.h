#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

extern void hpm_init(void);

extern void hpm_request(void);

extern void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

extern void hpm_enable(bool enable);

extern bool hpm_get(uint16_t * pm25, uint16_t * pm10);
