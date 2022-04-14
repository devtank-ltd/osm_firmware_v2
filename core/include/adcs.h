#pragma once

#include <stdint.h>

#include "config.h"


extern void adcs_init(void);

extern bool adcs_begin(uint8_t* channels, unsigned size);
extern bool adcs_collect_avgs(uint16_t* avgs, unsigned size);
extern bool adcs_collect_rms(uint16_t* rms, uint16_t midpoint, unsigned size, unsigned index);
extern bool adcs_collect_rmss(uint16_t* rmss, uint16_t* midpoints, unsigned size);
extern bool adcs_wait_done(uint32_t timeout);
