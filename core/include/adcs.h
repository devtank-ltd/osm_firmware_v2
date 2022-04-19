#pragma once

#include <stdint.h>

#include "config.h"


typedef enum
{
    ADCS_KEY_NONE,
    ADCS_KEY_CC,
    ADCS_KEY_BAT,
} adcs_keys_t;


typedef enum
{
    ADCS_RESP_OK,
    ADCS_RESP_FAIL,
    ADCS_RESP_WAIT,
} adcs_resp_t;


extern void adcs_init(void);

extern adcs_resp_t adcs_begin(uint8_t* channels, unsigned num_channels, unsigned num_samples, adcs_keys_t key);
extern adcs_resp_t adcs_collect_avgs(uint16_t* avgs, unsigned num_channels, unsigned num_samples, adcs_keys_t key, uint32_t* time_taken);
extern adcs_resp_t adcs_collect_rms(uint16_t* rms, uint16_t midpoint, unsigned num_channels, unsigned num_samples, unsigned cc_index, adcs_keys_t key, uint32_t* time_taken);
extern adcs_resp_t adcs_collect_rmss(uint16_t* rmss, uint16_t* midpoints, unsigned num_channels, unsigned num_samples, adcs_keys_t key, uint32_t* time_taken);
extern adcs_resp_t adcs_wait_done(uint32_t timeout, adcs_keys_t key);
extern void adcs_release(adcs_keys_t key);
