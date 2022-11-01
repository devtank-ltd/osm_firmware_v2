#pragma once
#include <stdbool.h>

#include "base_types.h"
#include "measurements.h"


extern void     pulsecount_init(void);

extern void     pulsecount_enable(bool enable);

extern void     pulsecount_log();

extern measurements_sensor_state_t  pulsecount_collection_time(char* name, uint32_t* collection_time);
extern measurements_sensor_state_t  pulsecount_begin(char* name, bool in_isolation);
extern measurements_sensor_state_t  pulsecount_get(char* name, measurements_reading_t* value);
extern void                         pulsecount_ack(char* name);
