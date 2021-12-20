#ifndef __ADCS__
#define __ADCS__


#include "measurements.h"


#define ADCS_SAMPLE_TIME    4000


extern void adcs_init(void);
extern void adcs_loop_iteration(void);

extern bool adcs_begin(char* name);
extern bool adcs_collect(char* name, value_t* value);
extern bool adcs_wait(value_t* value);

extern bool adcs_set_midpoint(uint16_t new_midpoint);
extern void adcs_calibrate_current_clamp(void);


#endif //__ADCS__
