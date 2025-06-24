#pragma once


#include <osm/core/base_types.h>
#include <osm/core/measurements.h>


void osm_senxx_iterate(void);
void osm_senxx_inf_init(osm_measurements_inf_t* inf);
struct osm_cmd_link_t* osm_senxx_add_commands(struct osm_cmd_link_t* tail);
void osm_senxx_init(void);
