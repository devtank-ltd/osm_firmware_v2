#ifndef __GPIOS_IN_OUT__
#define __GPIOS_IN_OUT__

#include <stdbool.h>


extern void     gpios_init();
extern unsigned gpios_get_count();
extern void     gpio_configure(unsigned gpio, bool as_input, int pull);
extern bool     gpio_is_input(unsigned gpio);
extern void     gpio_on(unsigned gpio, bool on_off);
extern void     gpio_log(unsigned gpio);
extern void     gpios_log();

extern unsigned inputs_get_count();
extern void     inputs_input_log(unsigned input);
extern void     inputs_log();

extern unsigned outputs_get_count();
extern void     outputs_set(unsigned output, bool on_off); 
extern void     output_output_log(unsigned output);
extern void     outputs_log();

#endif //__GPIOS_IN_OUT__
