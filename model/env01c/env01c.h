#pragma once

#include <stdint.h>

#include "base_types.h"
#include "uart_rings.h"


extern uint8_t  env01c_stm_adcs_get_channel(adcs_type_t adcs_type);
extern void     env01c_sensors_init(void);
extern void     env01c_persist_config_model_init(persist_env01c_config_v1_t * config);
extern void     env01c_post_init(void);
extern bool     env01c_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
extern bool     env01c_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
extern bool     env01c_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     env01c_debug_mode_enable_all(void);
extern void     env01c_measurements_repopulate(void);
extern void     env01c_cmds_add_all(struct cmd_link_t* tail);
extern unsigned env01c_measurements_add_defaults(measurements_def_t * measurements_arr);
