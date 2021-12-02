#pragma once

#include <stdbool.h>

#include "lorawan.h"


#define MEASUREMENTS__HEX_ARRAY_SIZE            26


typedef uint64_t value_t;


enum
{
    MEASUREMENT_UUID__TEMP  = 1 ,
    MEASUREMENT_UUID__HUM   = 2 ,
    MEASUREMENT_UUID__PM10  = 3 ,
    MEASUREMENT_UUID__PM25  = 4 ,
};


extern void measurements_init(void);
extern bool measurements_set_interval(char* name, uint8_t interval);
extern bool measurements_set_sample_rate(char* name, uint8_t sample_rate);
extern void measurements_loop(void);
