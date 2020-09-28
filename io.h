#ifndef __GPIOS_IN_OUT__
#define __GPIOS_IN_OUT__

#include <stdbool.h>


extern void     ios_init();
extern unsigned ios_get_count();
extern void     io_configure(unsigned io, bool as_input, unsigned pull);
extern bool     io_enable_special(unsigned io);
extern bool     io_is_input(unsigned io);
extern unsigned io_get_bias(unsigned io);
extern void     io_on(unsigned io, bool on_off);
extern void     io_log(unsigned io);
extern void     ios_log();

#endif //__GPIOS_IN_OUT__
