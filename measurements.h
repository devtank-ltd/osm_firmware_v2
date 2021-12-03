#pragma once

#include <stdbool.h>
#include <inttypes.h>

#include "lorawan.h"


typedef uint32_t value_t;


enum
{
    MEASUREMENT_UUID__TEMP  = 1 ,
    MEASUREMENT_UUID__HUM   = 2 ,
    MEASUREMENT_UUID__PM10  = 3 ,
    MEASUREMENT_UUID__PM25  = 4 ,
};


extern volatile bool measurement_trigger;

extern void measurements_init(void);
extern uint16_t measurements_num_measurements(void);
extern bool measurements_get_uuid(char* name, uint8_t* uuid);
extern bool measurements_set_interval(char* name, uint8_t interval);
extern bool measurements_set_interval_uuid(uint8_t uuid, uint8_t interval);
extern bool measurements_read_data_value(uint8_t uuid, value_t* val);
extern bool measurements_write_data_value(uint8_t uuid, value_t val);
extern void measurements_loop(void);

extern void measurements_send(uint32_t interval_count);
