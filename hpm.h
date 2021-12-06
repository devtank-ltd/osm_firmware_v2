#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "ring.h"
#include "measurements.h"


extern bool hpm_init(char* name);

extern void hpm_request(void);

extern void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

extern void hpm_enable(bool enable);

extern bool hpm_get(uint16_t * pm25, uint16_t * pm10);

extern bool hpm_get_pm25(char* name, value_t* val);
extern bool hpm_get_pm10(char* name, value_t* val);
