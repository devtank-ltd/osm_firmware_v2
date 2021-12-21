#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>


#include "measurements.h"


extern void init_persistent();
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);
extern void persist_set_lw_dev_eui(char* dev_eui);
extern bool persist_get_lw_dev_eui(char dev_eui[LW__DEV_EUI_LEN + 1]);
extern void persist_set_lw_app_key(char* app_key);
extern bool persist_get_lw_app_key(char app_key[LW__APP_KEY_LEN + 1]);

extern bool persist_get_measurements(measurement_def_base_t** m_arr);
extern void persist_commit_measurement(void);

extern uint8_t * persist_get_modbus_data(void);
extern void      persist_commit_modbus_data(void);

#endif //__PERSISTENT_CONIFG__
