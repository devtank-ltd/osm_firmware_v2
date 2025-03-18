#pragma once


#include <osm/core/base_types.h>
#include <osm/core/measurements.h>


void senxx_iterate(void);
void senxx_inf_init(measurements_inf_t* inf);
struct cmd_link_t* senxx_add_commands(struct cmd_link_t* tail);
void senxx_init(void);
