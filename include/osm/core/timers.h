#pragma once 

#include <inttypes.h>
#include <stdbool.h>

void     osm_timers_init();

void     osm_timer_delay_us(uint16_t wait_us);
void     osm_timer_delay_us_64(uint64_t wait_us);

void     osm_timer_wait();

bool     osm_timer_set_adc_boardary(unsigned ms);
unsigned osm_timer_get_adc_boardary();
