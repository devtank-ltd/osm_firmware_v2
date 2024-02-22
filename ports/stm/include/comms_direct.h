#pragma once

#include "base_types.h"


void passthrough_init(void);
struct cmd_link_t* passthrough_add_commands(struct cmd_link_t* tail);
