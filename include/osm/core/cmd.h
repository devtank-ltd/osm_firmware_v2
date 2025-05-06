#pragma once

#include <osm/core/base_types.h>

command_response_t cmds_process(char * command, unsigned len, cmd_ctx_t * ctx);

void cmds_init(void);

