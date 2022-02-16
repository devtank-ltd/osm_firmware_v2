#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "measurements.h"


extern void htu21d_init(void);

extern bool htu21d_read_temp(int32_t* temp);

extern bool htu21d_read_humidity(int32_t* humidity);

extern bool htu21d_read_dew_temp(int32_t* dew_temp);


extern measurements_sensor_state_t htu21d_measurements_collection_time(char* name, uint32_t* collection_time);

extern measurements_sensor_state_t htu21d_temp_measurements_init(char* name);
extern measurements_sensor_state_t htu21d_temp_measurements_get(char* name, value_t* value);

extern measurements_sensor_state_t htu21d_humi_measurements_init(char* name);
extern measurements_sensor_state_t htu21d_humi_measurements_get(char* name, value_t* value);
