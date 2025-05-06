#pragma once 

#include <inttypes.h>
#include <stdbool.h>

void     timers_init();

void     timer_delay_us(uint16_t wait_us);
void     timer_delay_us_64(uint64_t wait_us);

void     timer_wait();

bool     timer_set_adc_boardary(unsigned ms);
unsigned timer_get_adc_boardary();
