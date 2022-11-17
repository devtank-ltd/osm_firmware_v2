#pragma once


#include "measurements.h"


extern void ftma_init(void);
extern void ftma_inf_init(measurements_inf_t* inf);
extern struct cmd_link_t* ftma_add_commands(struct cmd_link_t* tail);
