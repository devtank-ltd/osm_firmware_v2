#ifndef __ADCS__
#define __ADCS__


#include "measurements.h"


extern void adcs_init(void);

extern void adcs_loop_iteration(void);

extern bool     adcs_begin(char* name);
extern bool     adcs_collect(char* name, value_t* value);
extern bool     adcs_get_cc(char* name, value_t* value);
extern bool     adcs_get_cc_blocking(char* name, value_t* value);
extern uint32_t adcs_collection_time(void);

extern bool     adcs_set_midpoint(uint16_t new_midpoint);
extern bool     adcs_calibrate_current_clamp(void);


#endif //__ADCS__
