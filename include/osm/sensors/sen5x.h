#pragma once


#include <osm/core/base_types.h>
#include <osm/core/measurements.h>


extern void                         sen5x_iterate(void);
extern void                         sen5x_inf_init(measurements_inf_t* inf);
extern struct cmd_link_t*           sen5x_add_commands(struct cmd_link_t* tail);
extern void                         sen5x_init(void);
