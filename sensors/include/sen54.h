#pragma once


#include "base_types.h"
#include "measurements.h"


extern void                         sen54_inf_init(measurements_inf_t* inf);
extern struct cmd_link_t*           sen54_add_commands(struct cmd_link_t* tail);
extern void                         sen54_init(void);
