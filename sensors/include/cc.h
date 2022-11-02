#pragma once


#include "measurements.h"


extern void                         cc_init(void);

extern bool                         cc_get_blocking(char* name, measurements_reading_t* value);
extern bool                         cc_get_all_blocking(measurements_reading_t* value_1, measurements_reading_t* value_2, measurements_reading_t* value_3);

extern bool                         cc_set_midpoint(uint32_t midpoint, char* name);
extern bool                         cc_get_midpoint(uint32_t* midpoint, char* name);
extern bool                         cc_calibrate(void);

extern bool                         cc_set_active_clamps(adcs_type_t* clamps, unsigned len);

extern void                         cc_inf_init(measurements_inf_t* inf);
