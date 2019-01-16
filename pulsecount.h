#ifndef __PULSECOUNT__

extern void pulsecount_init();

extern void pulsecount_do_samples();
extern void pulsecount_second_boardary();

extern void pulsecount_A_get(unsigned * pps, unsigned * min_v, unsigned * max_v);

extern void pulsecount_B_get(unsigned * pps);

#endif //__PULSECOUNT__
