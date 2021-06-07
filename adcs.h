#ifndef __ADCS__
#define __ADCS__

extern char adc_temp_buffer[24];

extern void     adcs_init();

extern void     adcs_do_samples();
extern void     adcs_second_boardary();

extern unsigned adcs_get_count();

extern unsigned adcs_get_last(unsigned adc);
extern unsigned adcs_get_tick(unsigned adc);

extern void     adcs_adc_log(unsigned adc);
extern void     adcs_log();


#endif //__ADCS__
