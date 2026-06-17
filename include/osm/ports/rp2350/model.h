#pragma once

#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/uart_rings.h>
#include "model_config.h"


void     osm_model_sensors_init(void);
void     osm_model_persist_config_model_init(osm_persist_model_config_t * config);
bool     osm_model_persist_config_cmp(osm_persist_model_config_t* d0, osm_persist_model_config_t* d1);
void     osm_model_post_init(void);
bool     osm_model_uart_ring_done_in_process(unsigned uart, osm_ring_buf_t * ring);
bool     osm_model_uart_ring_do_out_drain(unsigned uart, osm_ring_buf_t * ring);
bool     osm_model_measurements_get_inf(osm_measurements_def_t * def, osm_measurements_data_t* data, osm_measurements_inf_t* inf);
void     osm_model_measurements_repopulate(void);
void     osm_model_cmds_add_all(struct osm_cmd_link_t* tail);
void     osm_model_uarts_setup(void);
unsigned osm_model_measurements_add_defaults(osm_measurements_def_t * measurements_arr);
void     osm_model_main_loop_iterate(void);
bool     osm_model_config_update(const void* from_config, osm_persist_model_config_t* to_config, uint16_t from_model_version);
