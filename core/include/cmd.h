#pragma once

#include "base_types.h"

extern command_response_t cmds_process(char * command, unsigned len, cmd_output_t cmd_output);

extern void cmds_init(void);

