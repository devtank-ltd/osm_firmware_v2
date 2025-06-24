#pragma once


#include <osm/core/measurements.h>


#define OSM_SAI_DEFAULT_NO_BUF                                              100


void                         osm_sai_init(void);
bool                         osm_sai_set_coeff(uint8_t index, float coeff);
void                         osm_sai_print_coeffs(osm_cmd_ctx_t * ctx);

void                                osm_sai_inf_init(osm_measurements_inf_t* inf);
struct osm_cmd_link_t*           osm_sai_add_commands(struct osm_cmd_link_t* tail);
