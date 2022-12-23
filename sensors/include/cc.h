#pragma once


#include "measurements.h"


typedef struct
{
    uint32_t midpoint;
    uint32_t ext_max_mA;
    uint32_t int_max_mV;
} cc_config_t;


extern void                         cc_init(void);

extern bool                         cc_get_blocking(char* name, measurements_reading_t* value);
extern bool                         cc_get_all_blocking(measurements_reading_t* value_1, measurements_reading_t* value_2, measurements_reading_t* value_3);

extern bool                         cc_set_active_clamps(adcs_type_t* clamps, unsigned len);

extern void                         cc_inf_init(measurements_inf_t* inf);

extern struct cmd_link_t*           cc_add_commands(struct cmd_link_t* tail);

extern void                         cc_setup_default_mem(cc_config_t* memory, unsigned size);
