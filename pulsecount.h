#ifndef __PULSECOUNT__

extern void pulsecount_init();

extern void pulsecount_do_samples();
extern void pulsecount_second_boardary();

extern void pulsecount_1_get(unsigned * pps);
extern void pulsecount_2_get(unsigned * pps);

#endif //__PULSECOUNT__
