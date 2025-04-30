#pragma once


#include <osm/core/base_types.h>
#include <osm/core/measurements.h>


void                         sen5x_iterate(void);
void                         sen5x_inf_init(measurements_inf_t* inf);
struct cmd_link_t*           sen5x_add_commands(struct cmd_link_t* tail);
void                         sen5x_init(void);
