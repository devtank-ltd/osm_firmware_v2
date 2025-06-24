#pragma once

#include <osm/core/base_types.h>


void osm_comms_direct_init(void);
void osm_comms_direct_loop_iterate(void);
struct osm_cmd_link_t* osm_comms_direct_add_commands(struct osm_cmd_link_t* tail);
