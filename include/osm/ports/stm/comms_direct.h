#pragma once

#include <osm/core/base_types.h>


void osm_comms_direct_init(void);
void osm_comms_direct_loop_iterate(void);
struct cmd_link_t* osm_comms_direct_add_commands(struct cmd_link_t* tail);
