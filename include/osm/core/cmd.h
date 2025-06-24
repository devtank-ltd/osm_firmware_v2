#pragma once

#include <osm/core/base_types.h>

osm_command_response_t osm_cmds_process(char * command, unsigned len, cmd_ctx_t * ctx);

void osm_cmds_init(void);

