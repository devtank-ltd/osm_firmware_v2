#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "ring.h"
#include "measurements.h"

extern void hpm_request(void);

extern void hpm_ring_process(ring_buf_t * ring, char * tmpbuf, unsigned tmpbuf_len);

extern void hpm_enable(bool enable);

extern bool hpm_get(uint16_t * pm25, uint16_t * pm10);


extern measurements_sensor_state_t hpm_init(char* name);
extern measurements_sensor_state_t hpm_get_pm25(char* name, measurements_reading_t* val);
extern measurements_sensor_state_t hpm_get_pm10(char* name, measurements_reading_t* val);
extern measurements_sensor_state_t hpm_collection_time(char* name, uint32_t* collection_time);
