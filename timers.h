#ifndef __TIMERS__
#define __TIMERS__

extern void     timers_init();

extern void     timers_fast_set_rate(unsigned count);
extern unsigned timers_fast_get_rate();
extern void     timers_fast_start();
extern bool     timers_fast_is_running();
extern void     timers_fast_stop();

#endif //__TIMERS__
