#pragma once


#include <osm/core/measurements.h>


#define OSM_SAI_DEFAULT_NO_BUF                                              100


void                         osm_sai_init(void);
bool                         osm_sai_set_coeff(uint8_t index, float coeff);
void                         osm_sai_print_coeffs(cmd_ctx_t * ctx);

void                                osm_sai_inf_init(measurements_inf_t* inf);
struct cmd_link_t*           osm_sai_add_commands(struct cmd_link_t* tail);
