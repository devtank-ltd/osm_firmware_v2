#ifndef __PERSISTENT_CONIFG__
#define __PERSISTENT_CONIFG__

#include <stdint.h>
#include <stdbool.h>

extern void init_persistent();
extern void persist_set_log_debug_mask(uint32_t mask);
extern uint32_t persist_get_log_debug_mask(void);
extern void persist_set_lw_dev_eui(char* dev_eui);
extern void persist_get_lw_dev_eui(char* dev_eui);
extern void persist_set_lw_app_key(char* app_key);
extern void persist_get_lw_app_key(char* app_key);
extern bool persist_set_interval(uint8_t uuid, uint16_t interval);
extern bool persist_get_interval(uint8_t uuid, uint16_t* interval);
extern void persist_new_interval(uint8_t uuid, uint16_t interval);

#endif //__PERSISTENT_CONIFG__
