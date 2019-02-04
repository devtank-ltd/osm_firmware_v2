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

extern void     adcs_start_read(unsigned adc);
extern bool     adcs_async_read_complete(unsigned *value);

extern void     adcs_enable_sampling(bool enabled);
extern bool     adcs_is_enabled();

extern void     adcs_adc_log(unsigned adc);
extern void     adcs_log();


#endif //__ADCS__
