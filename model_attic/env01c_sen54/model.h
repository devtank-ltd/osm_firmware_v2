#pragma once

#include <stdint.h>

#include "base_types.h"
#include "uart_rings.h"


extern uint8_t  env01c_sen54_stm_adcs_get_channel(adcs_type_t adcs_type);
extern void     env01c_sen54_sensors_init(void);
extern void     env01c_sen54_persist_config_model_init(persist_env01c_sen54_config_v1_t * config);
extern bool     env01c_sen54_persist_config_cmp(persist_env01c_sen54_config_v1_t* d0, persist_env01c_sen54_config_v1_t* d1);
extern void     env01c_sen54_post_init(void);
extern bool     env01c_sen54_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
extern bool     env01c_sen54_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
extern bool     env01c_sen54_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     env01c_sen54_measurements_repopulate(void);
extern void     env01c_sen54_cmds_add_all(struct cmd_link_t* tail);
extern void     env01c_sen54_w1_pulse_enable_pupd(unsigned io, bool enabled);
extern bool     env01c_sen54_can_io_be_special(unsigned io, io_special_t special);
extern void     env01c_sen54_uarts_setup(void);
extern void     env01c_sen54_setup_pulse_pupd(uint8_t* pupd);
extern unsigned env01c_sen54_measurements_add_defaults(measurements_def_t * measurements_arr);
