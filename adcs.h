#ifndef __ADCS__
#define __ADCS__


extern char adc_temp_buffer[24];

extern void     adcs_init(void);

extern void     adcs_do_samples(void);
extern void     adcs_collect_boardary(void);

extern unsigned adcs_get_count(void);

extern unsigned adcs_get_last(unsigned adc);
extern unsigned adcs_get_tick(unsigned adc);

extern void     adcs_adc_log(unsigned adc);
extern void     adcs_log(void);

extern bool adcs_set_midpoint(uint16_t new_midpoint);
extern void adcs_calibrate_current_clamp(void);
extern void adcs_cb(char* args);


#endif //__ADCS__
