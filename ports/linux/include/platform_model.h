#pragma once
#include "model.h"

#define osm_main main

#define model_persist_config_model_init    CONCAT(fw_name, _persist_config_model_init)
#define model_persist_config_cmp           CONCAT(fw_name, _persist_config_cmp)
#define model_sensors_init                 CONCAT(fw_name,_sensors_init)
#define model_post_init                    CONCAT(fw_name,_post_init)
#define model_uart_ring_done_in_process    CONCAT(fw_name,_uart_ring_done_in_process)
#define model_uart_ring_do_out_drain       CONCAT(fw_name,_uart_ring_do_out_drain)
#define model_measurements_get_inf         CONCAT(fw_name,_measurements_get_inf)
#define model_debug_mode_enable_all        CONCAT(fw_name,_debug_mode_enable_all)
#define model_measurements_repopulate      CONCAT(fw_name,_measurements_repopulate)
#define model_cmds_add_all                 CONCAT(fw_name,_cmds_add_all)
#define model_w1_pulse_enable_pupd         CONCAT(fw_name,_w1_pulse_enable_pupd)
#define model_can_io_be_special            CONCAT(fw_name,_can_io_be_special)
#define model_measurements_add_defaults    CONCAT(fw_name,_measurements_add_defaults)
#define model_linux_spawn_fakes            CONCAT(fw_name,_linux_spawn_fakes)
#define model_linux_close_fakes            CONCAT(fw_name,_linux_close_fakes)

#define MODEL_NAME STR(FW_NAME)
