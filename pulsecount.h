#pragma once
#include <stdbool.h>
#include "value.h"

extern void     pulsecount_init(void);

extern void     pulsecount_enable(bool enable);

extern void     pulsecount_log();

extern uint32_t pulsecount_collection_time(void);
extern bool     pulsecount_begin(char* name);
extern bool     pulsecount_get(char* name, value_t* value);
extern void     pulsecount_ack(void);
