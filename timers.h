#pragma once 

#include <inttypes.h>
#include <stdbool.h>

extern void     timers_init();

extern void     timer_delay_us(uint16_t wait_us);
extern void     timer_delay_us_64(uint64_t wait_us);

extern void     timer_wait();

extern bool     timer_set_adc_boardary(unsigned ms);
extern unsigned timer_get_adc_boardary();
