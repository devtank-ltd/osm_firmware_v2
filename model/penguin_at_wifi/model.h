#pragma once

#include <stdint.h>

#include "base_types.h"
#include "uart_rings.h"


extern void     penguin_at_wifi_sensors_init(void);
extern void     penguin_at_wifi_persist_config_model_init(persist_penguin_at_wifi_config_v1_t * config);
extern bool     penguin_at_wifi_persist_config_cmp(persist_penguin_at_wifi_config_v1_t* d0, persist_penguin_at_wifi_config_v1_t* d1);
extern void     penguin_at_wifi_post_init(void);
extern bool     penguin_at_wifi_uart_ring_done_in_process(unsigned uart, ring_buf_t * ring);
extern bool     penguin_at_wifi_uart_ring_do_out_drain(unsigned uart, ring_buf_t * ring);
extern bool     penguin_at_wifi_measurements_get_inf(measurements_def_t * def, measurements_data_t* data, measurements_inf_t* inf);
extern void     penguin_at_wifi_debug_mode_enable_all(void);
extern void     penguin_at_wifi_measurements_repopulate(void);
extern void     penguin_at_wifi_cmds_add_all(struct cmd_link_t* tail);
extern void     penguin_at_wifi_w1_pulse_enable_pupd(unsigned io, bool enabled);
extern bool     penguin_at_wifi_can_io_be_special(unsigned io, io_special_t special);
extern unsigned penguin_at_wifi_measurements_add_defaults(measurements_def_t * measurements_arr);
extern void     penguin_at_wifi_linux_spawn_fakes(void);
extern void     penguin_at_wifi_linux_close_fakes(void);
