#pragma once


#include "measurements.h"


extern void                         sai_init(void);
extern bool                         sai_get_sound(uint32_t* dB);
extern bool                         sai_set_coeff(uint8_t index, float coeff);
extern void                         sai_print_coeffs(void);

extern measurements_sensor_state_t  sai_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  sai_iteration_callback(char* name);
extern measurements_sensor_state_t  sai_measurements_init(char* name);
extern measurements_sensor_state_t  sai_measurements_get(char* name, value_t* value);
