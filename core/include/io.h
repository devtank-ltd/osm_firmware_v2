#pragma once

#include <stdbool.h>


extern bool     ios_get_pupd(unsigned io, uint8_t* pupd);


extern void     ios_init(void);
extern unsigned ios_get_count(void);
extern void     io_configure(unsigned io, bool as_input, unsigned pull);


extern bool     io_enable_pulsecount(unsigned io, uint8_t pupd);
extern bool     io_enable_w1(unsigned io);

extern bool     io_is_pulsecount_now(unsigned io);
extern bool     io_is_w1_now(unsigned io);

extern bool     io_is_input(unsigned io);
extern unsigned io_get_bias(unsigned io);
extern void     io_on(unsigned io, bool on_off);
extern void     io_log(unsigned io);
extern void     ios_log();
