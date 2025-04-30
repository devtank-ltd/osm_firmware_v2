#pragma once


#include <osm/core/measurements.h>
#include <osm/core/config.h>


typedef struct
{
    char        name[MEASURE_NAME_NULLED_LEN];
    float       coeffs[FTMA_NUM_COEFFS];
} ftma_config_t;


extern void ftma_init(void);
extern void ftma_inf_init(measurements_inf_t* inf);
extern struct cmd_link_t* ftma_add_commands(struct cmd_link_t* tail);
extern void ftma_setup_default_mem(ftma_config_t* memory, unsigned size);
