#pragma once

#include <stdbool.h>
#include <stdint.h>

extern void veml7700_init(void);

extern bool veml7700_read_light(int32_t* light);


extern uint32_t veml7700_measurements_collection_time(void);

extern bool veml7700_light_measurements_init(char* name);
extern bool veml7700_light_measurements_get(char* name, value_t* value);
