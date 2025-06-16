#pragma once


#include <osm/core/base_types.h>
#include <osm/core/measurements.h>


#define OSM_CC_TYPE_A               'A'
#define OSM_CC_TYPE_V               'V'


typedef struct
{
    uint32_t midpoint;
    uint32_t ext_max_mA;
    uint32_t int_max_mV;
    char     type;
} cc_config_t;


void                         osm_cc_init(void);

bool                         osm_cc_get_blocking(char* name, measurements_reading_t* value);
bool                         osm_cc_get_all_blocking(measurements_reading_t* value_1, measurements_reading_t* value_2, measurements_reading_t* value_3);

bool                         osm_cc_set_active_clamps(adcs_type_t* clamps, unsigned len);

void                         osm_cc_inf_init(measurements_inf_t* inf);

struct cmd_link_t*           osm_cc_add_commands(struct cmd_link_t* tail);

void                         osm_cc_setup_default_mem(cc_config_t* memory, unsigned size);
