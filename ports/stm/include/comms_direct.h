#pragma once

#include "base_types.h"


void comms_direct_init(void);
struct cmd_link_t* comms_direct_add_commands(struct cmd_link_t* tail);
