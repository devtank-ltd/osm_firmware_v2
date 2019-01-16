#ifndef __ADCS__
#define __ADCS__

extern void     adcs_init();

extern void     adcs_do_samples();
extern void     adcs_second_boardary();

extern void     adcs_get(unsigned   adc,
                         unsigned * min_value,
                         unsigned * max_value,
                         double *   av_value);

extern unsigned adcs_get_count();

extern void     adcs_log();


#endif //__ADCS__
