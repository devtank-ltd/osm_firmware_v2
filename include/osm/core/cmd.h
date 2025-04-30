#pragma once

#include <osm/core/base_types.h>

extern command_response_t cmds_process(char * command, unsigned len, cmd_ctx_t * ctx);

extern void cmds_init(void);

