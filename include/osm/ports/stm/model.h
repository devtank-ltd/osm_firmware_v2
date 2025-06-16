#pragma once

#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/uart_rings.h>
#include "model_config.h"


uint8_t  osm_model_stm_adcs_get_channel(adcs_type_t adcs_type);
void     osm_model_sensors_init(void);
void     osm_model_persist_config_model_init(persist_model_config_t * config);
bool     osm_model_persist_config_cmp(persist_model_config_t* d0, persist_model_config_t* d1);
void     osm_model_post_init(void);
bool     osm_model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
bool     osm_model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
bool     osm_model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
void     osm_model_measurements_repopulate(void);
void     osm_model_cmds_add_all(struct cmd_link_t* tail);
void     osm_model_w1_pulse_enable_pupd(unsigned io, bool enabled);
bool     osm_model_can_io_be_special(unsigned io, io_special_t special);
void     osm_model_uarts_setup(void);
void     osm_model_setup_pulse_pupd(uint8_t* pupd);
unsigned osm_model_measurements_add_defaults(measurements_def_t * measurements_arr);
void     osm_model_main_loop_iterate(void);
bool     osm_model_config_update(const void* from_config, persist_model_config_t* to_config, uint16_t from_model_version);
