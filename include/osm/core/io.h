#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include "pinmap.h"
#include <osm/core/persist_config.h>

#define IOS_MEASUREMENT_NAME_PRE            "IO"


typedef enum
{
    IO_PUPD_NONE    = 0,
    IO_PUPD_UP      = 1,
    IO_PUPD_DOWN    = 2,
} io_pupd_t;


extern const port_n_pins_t ios_pins[IOS_COUNT];


bool     ios_get_pupd(unsigned io, uint8_t* pupd);


void     ios_init(void);
void     ios_post_init(void);
unsigned ios_get_count(void);
void     io_configure(unsigned io, bool as_input, io_pupd_t pull);


bool     io_enable_pulsecount(unsigned io, io_pupd_t pupd, io_special_t edge);
bool     io_enable_w1(unsigned io);

bool     io_is_pulsecount_now(unsigned io);
bool     io_is_w1_now(unsigned io);
bool     io_is_watch_now(unsigned io);

bool     io_is_input(unsigned io);
unsigned io_get_bias(unsigned io);
void     io_on(unsigned io, bool on_off);
void     io_log(unsigned io, cmd_ctx_t * ctx);
void     ios_log(cmd_ctx_t * ctx);

struct cmd_link_t* ios_add_commands(struct cmd_link_t* tail);

void     ios_inf_init(measurements_inf_t* inf);
void     ios_measurements_init(void);
