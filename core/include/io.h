#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"
#include "measurements.h"


typedef enum
{
    IO_PUPD_NONE    = 0,
    IO_PUPD_UP      = 1,
    IO_PUPD_DOWN    = 2,
} io_pupd_t;


extern bool     ios_get_pupd(unsigned io, uint8_t* pupd);


extern void     ios_init(void);
extern unsigned ios_get_count(void);
extern void     io_configure(unsigned io, bool as_input, io_pupd_t pull);


extern bool     io_enable_pulsecount(unsigned io, io_pupd_t pupd, io_special_t edge);
extern bool     io_enable_w1(unsigned io);

extern bool     io_is_pulsecount_now(unsigned io);
extern bool     io_is_w1_now(unsigned io);

extern bool     io_is_input(unsigned io);
extern unsigned io_get_bias(unsigned io);
extern void     io_on(unsigned io, bool on_off);
extern void     io_log(unsigned io);
extern void     ios_log();

extern struct cmd_link_t* ios_add_commands(struct cmd_link_t* tail);

extern void     ios_inf_init(measurements_inf_t* inf);
extern void     ios_measurements_init(void);
