#pragma once


#include <osm/core/measurements.h>


#define SAI_DEFAULT_NO_BUF                                              100


void                         sai_init(void);
bool                         sai_set_coeff(uint8_t index, float coeff);
void                         sai_print_coeffs(cmd_ctx_t * ctx);

void                                sai_inf_init(measurements_inf_t* inf);
struct cmd_link_t*           sai_add_commands(struct cmd_link_t* tail);
