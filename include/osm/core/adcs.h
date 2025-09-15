#pragma once

#include <stdint.h>

#include <osm/core/config.h>
#include <osm/core/base_types.h>


typedef enum
{
    OSM_ADCS_KEY_NONE,
    OSM_ADCS_KEY_CC,
    OSM_ADCS_KEY_BAT,
    OSM_ADCS_KEY_FTMA,
} osm_adcs_keys_t;


typedef enum
{
    OSM_ADCS_RESP_OK,
    OSM_ADCS_RESP_FAIL,
    OSM_ADCS_RESP_WAIT,
} osm_adcs_resp_t;


bool osm_adcs_to_mV(uint32_t value, uint32_t* mV);

void osm_adcs_off(void);
void osm_adcs_init(void);

osm_adcs_resp_t osm_adcs_begin(osm_adcs_type_t* channels, unsigned num_channels, unsigned num_samples, osm_adcs_keys_t key);
osm_adcs_resp_t osm_adcs_collect_avg(uint32_t* avg, unsigned num_channels, unsigned num_samples, unsigned index, osm_adcs_keys_t key, uint32_t* time_taken);
osm_adcs_resp_t osm_adcs_collect_avgs(uint32_t* avgs, unsigned num_channels, unsigned num_samples, osm_adcs_keys_t key, uint32_t* time_taken);
osm_adcs_resp_t osm_adcs_collect_rms(uint32_t* rms, uint32_t midpoint, unsigned num_channels, unsigned num_samples, unsigned cc_index, osm_adcs_keys_t key, uint32_t* time_taken);
osm_adcs_resp_t osm_adcs_collect_rmss(uint32_t* rmss, uint32_t* midpoints, unsigned num_channels, unsigned num_samples, osm_adcs_keys_t key, uint32_t* time_taken);
osm_adcs_resp_t osm_adcs_wait_done(uint32_t timeout, osm_adcs_keys_t key);
void osm_adcs_release(osm_adcs_keys_t key);

void osm_adcs_dma_complete(void);
