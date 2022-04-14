#pragma once

#include <stdint.h>

#include "config.h"


typedef enum
{
    ADCS_KEY_NONE,
    ADCS_KEY_CC,
    ADCS_KEY_BAT,
} adcs_keys_t;


extern void adcs_init(void);

extern bool adcs_begin(uint8_t* channels, unsigned size, adcs_keys_t key);
extern bool adcs_collect_avgs(uint16_t* avgs, unsigned size, adcs_keys_t key);
extern bool adcs_collect_rms(uint16_t* rms, uint16_t midpoint, unsigned size, unsigned cc_index, adcs_keys_t key);
extern bool adcs_collect_rmss(uint16_t* rmss, uint16_t* midpoints, unsigned size, adcs_keys_t key);
extern bool adcs_wait_done(uint32_t timeout, adcs_keys_t key);
