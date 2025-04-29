#pragma once

#include <stdint.h>

#include "base_types.h"
#include "uart_rings.h"
#include "model_config.h"


extern uint8_t  model_stm_adcs_get_channel(adcs_type_t adcs_type);
extern void     model_sensors_init(void);
extern void     model_persist_config_model_init(persist_model_config_t * config);
extern bool     model_persist_config_cmp(persist_model_config_t* d0, persist_model_config_t* d1);
extern void     model_post_init(void);
extern bool     model_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
extern bool     model_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
extern bool     model_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     model_measurements_repopulate(void);
extern void     model_cmds_add_all(struct cmd_link_t* tail);
extern void     model_w1_pulse_enable_pupd(unsigned io, bool enabled);
extern bool     model_can_io_be_special(unsigned io, io_special_t special);
extern void     model_uarts_setup(void);
extern void     model_setup_pulse_pupd(uint8_t* pupd);
extern unsigned model_measurements_add_defaults(measurements_def_t * measurements_arr);
extern void     model_main_loop_iterate(void);
extern bool     model_config_update(persist_model_config_t* config, uint16_t from_model_version);
