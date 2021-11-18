#pragma once

#include <stdbool.h>

#include "lorawan.h"


typedef uint32_t value_t;


enum
{
    MEASUREMENT_UUID__TEMP  = 1,
    MEASUREMENT_UUID__HUM   = 2,
};


extern volatile bool measurement_trigger;

extern void measurements_init(void);
extern bool measurements_set_interval(char* name, uint8_t interval);
extern bool measurements_read_data_value(uint8_t uuid, value_t* val);
extern bool measurements_write_data_value(uint8_t uuid, value_t val);
extern void measurements_loop(void);