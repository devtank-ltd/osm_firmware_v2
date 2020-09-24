#ifndef __PULSECOUNT__

extern void     pulsecount_init();

extern void     pulsecount_second_boardary();

extern unsigned pulsecount_get_count();

extern void     pulsecount_pps_log(unsigned pps);

extern void     pulsecount_log();

#endif //__PULSECOUNT__
