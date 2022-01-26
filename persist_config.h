#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "measurements.h"


extern void persistent_init();
extern void persist_set_fw_ready(uint32_t size);
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);

extern void persist_set_lw_dev_eui(char* dev_eui);
extern bool persist_get_lw_dev_eui(char dev_eui[LW__DEV_EUI_LEN + 1]);
extern void persist_set_lw_app_key(char* app_key);
extern bool persist_get_lw_app_key(char app_key[LW__APP_KEY_LEN + 1]);

extern bool persist_get_measurements(measurement_def_base_t** arr);
extern void persist_set_measurements(measurement_def_t * arr);

extern bool persist_set_adc_midpoint(uint16_t midpoint);
extern bool persist_get_adc_midpoint(uint16_t* midpoint);

extern uint8_t * persist_get_modbus_data(void);
extern void      persist_commit_modbus_data(void);

extern void persistent_wipe();
