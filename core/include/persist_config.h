#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "measurements.h"
#include "pinmap.h"


extern void persistent_init(void);

extern void persist_commit();

extern void persist_set_fw_ready(uint32_t size);
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);

extern void persist_set_lw_dev_eui(char* dev_eui);
extern bool persist_get_lw_dev_eui(char dev_eui[LW_DEV_EUI_LEN + 1]);
extern void persist_set_lw_app_key(char* app_key);
extern bool persist_get_lw_app_key(char app_key[LW_APP_KEY_LEN + 1]);

extern measurements_def_t * persist_get_measurements(void);

extern bool persist_get_mins_interval(uint32_t * mins_interval);
extern bool persist_set_mins_interval(uint32_t mins_interval);

extern bool persist_set_cc_midpoints(uint32_t midpoints[ADC_CC_COUNT]);
extern bool persist_get_cc_midpoints(uint32_t midpoints[ADC_CC_COUNT]);

extern float * persist_get_sai_cal_coeffs(void);

extern uint16_t* persist_get_ios_state(void);

extern modbus_bus_t * persist_get_modbus_bus(void);

extern void persistent_wipe();

extern cc_config_t * persist_get_cc_configs(void);

extern char* persist_get_serial_number(void);
