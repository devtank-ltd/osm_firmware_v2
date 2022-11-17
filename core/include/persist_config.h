#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "base_types.h"
#include "measurements.h"
#include "pinmap.h"
#include "types.h"


extern void persistent_init(void);

extern void persist_commit();

extern void persist_set_fw_ready(uint32_t size);
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);

extern comms_config_t* persist_get_comms_config(void);

extern measurements_def_t * persist_get_measurements(void);

extern bool persist_get_mins_interval(uint32_t * mins_interval);
extern bool persist_set_mins_interval(uint32_t mins_interval);

extern float * persist_get_sai_cal_coeffs(void);

extern uint16_t* persist_get_ios_state(void);

extern modbus_bus_t * persist_get_modbus_bus(void);

extern void persistent_wipe();

extern char* persist_get_serial_number(void);

extern adc_persist_config_t* persist_get_adc_config(void);

extern struct cmd_link_t* persist_config_add_commands(struct cmd_link_t* tail);
