#ifndef __ADCS__
#define __ADCS__

extern void     adcs_init(void);

extern bool adcs_set_midpoint(uint16_t new_midpoint);
extern void adcs_calibrate_current_clamp(void);
extern void adcs_cb(char* args);


#endif //__ADCS__
