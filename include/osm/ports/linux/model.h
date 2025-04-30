#pragma once

#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/uart_rings.h>
#include "model_config.h"


void     model_sensors_init(void);
void     model_persist_config_model_init(persist_model_config_t * config);
bool     model_persist_config_cmp(persist_model_config_t* d0, persist_model_config_t* d1);
void     model_post_init(void);
bool     model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
bool     model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
bool     model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
void     model_measurements_repopulate(void);
void     model_cmds_add_all(struct cmd_link_t* tail);
void     model_w1_pulse_enable_pupd(unsigned io, bool enabled);
bool     model_can_io_be_special(unsigned io, io_special_t special);
unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr);
void     model_linux_spawn_fakes(void);
void     model_linux_close_fakes(void);
void     model_main_loop_iterate(void);
bool     model_config_update(const void* from_config, persist_model_config_t* to_config, uint16_t from_model_version);
