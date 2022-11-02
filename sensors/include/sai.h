#pragma once


#include "measurements.h"


extern void                         sai_init(void);
extern bool                         sai_set_coeff(uint8_t index, float coeff);
extern void                         sai_print_coeffs(void);

void                                sai_inf_init(measurements_inf_t* inf);
