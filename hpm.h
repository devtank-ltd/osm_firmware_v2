#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ring.h"

extern void hpm_init(void);

extern void hmp_request(void);

extern void hdm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

extern void hmp_enable(bool enable);
