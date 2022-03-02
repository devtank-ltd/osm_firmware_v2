#pragma once
#include <stdbool.h>

#include "measurements.h"

extern void     pulsecount_init(void);

extern void     pulsecount_enable(bool enable);

extern void     pulsecount_log();

extern measurements_sensor_state_t  pulsecount_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  pulsecount_begin(char* name);
extern measurements_sensor_state_t  pulsecount_get(char* name, value_t* value);
extern void                         pulsecount_ack(void);
