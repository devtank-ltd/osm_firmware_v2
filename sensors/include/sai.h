#pragma once


#include "measurements.h"


#define SAI_DEFAULT_NO_BUF                                              100


extern void                         sai_init(void);
extern bool                         sai_set_coeff(uint8_t index, float coeff);
extern void                         sai_print_coeffs(void);

void                                sai_inf_init(measurements_inf_t* inf);
extern struct cmd_link_t*           sai_add_commands(struct cmd_link_t* tail);
