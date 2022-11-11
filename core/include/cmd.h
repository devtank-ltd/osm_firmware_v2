#pragma once

#include "base_types.h"


extern void cmds_process(char * command, unsigned len);

extern void cmds_init(void);

extern void cmds_add_all(struct cmd_link_t* tail) __attribute__((weak));
