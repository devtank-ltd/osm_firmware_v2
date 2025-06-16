#pragma once


#include <osm/core/measurements.h>
#include <osm/core/config.h>


typedef struct
{
    char        name[OSM_MEASURE_NAME_NULLED_LEN];
    float       coeffs[OSM_FTMA_NUM_COEFFS];
} ftma_config_t;


void osm_ftma_init(void);
void osm_ftma_inf_init(measurements_inf_t* inf);
struct cmd_link_t* osm_ftma_add_commands(struct cmd_link_t* tail);
void osm_ftma_setup_default_mem(ftma_config_t* memory, unsigned size);
