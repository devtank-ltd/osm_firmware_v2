#pragma once

extern bool w1_query_temp(double* temperature);

extern bool     w1_measurement_init(char* name);
extern bool     w1_measurement_collect(char* name, value_t* value);
extern uint32_t w1_collection_time(void);

extern void     w1_temp_init(void);
