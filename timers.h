#ifndef __TIMERS__
#define __TIMERS__

extern void     timers_init();

extern void     timer_wait();

extern bool     timer_set_adc_boardary(unsigned ms);
extern unsigned timer_get_adc_boardary();

#endif //__TIMERS__
