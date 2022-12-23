#pragma once

#include <stdint.h>

#include "base_types.h"
#include "uart_rings.h"


extern uint8_t  env01_stm_adcs_get_channel(adcs_type_t adcs_type);
extern void     env01_sensors_init(void);
extern void     env01_persist_config_model_init(persist_env01_config_v1_t * config);
extern void     env01_post_init(void);
extern bool     env01_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
extern bool     env01_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
extern bool     env01_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     env01_debug_mode_enable_all(void);
extern void     env01_measurements_repopulate(void);
extern void     env01_cmds_add_all(struct cmd_link_t* tail);
extern unsigned env01_measurements_add_defaults(measurements_def_t * measurements_arr);
