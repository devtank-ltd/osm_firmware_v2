#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include "pinmap.h"
#include <osm/core/persist_config.h>

#define OSM_IOS_MEASUREMENT_NAME_PRE            "IO"


typedef enum
{
    OSM_IO_PUPD_NONE    = 0,
    OSM_IO_PUPD_UP      = 1,
    OSM_IO_PUPD_DOWN    = 2,
} osm_io_pupd_t;


extern const osm_port_n_pins_t ios_pins[IOS_COUNT];


bool     osm_ios_get_pupd(unsigned io, uint8_t* pupd);


void     osm_ios_init(void);
void     osm_ios_post_init(void);
unsigned osm_ios_get_count(void);
void     osm_io_configure(unsigned io, bool as_input, osm_io_pupd_t pull);


bool     osm_io_enable_pulsecount(unsigned io, osm_io_pupd_t pupd, osm_io_special_t edge);
bool     osm_io_enable_w1(unsigned io);

bool     osm_io_is_pulsecount_now(unsigned io);
bool     osm_io_is_w1_now(unsigned io);
bool     osm_io_is_watch_now(unsigned io);

bool     osm_io_is_input(unsigned io);
unsigned osm_io_get_bias(unsigned io);
void     osm_io_on(unsigned io, bool on_off);
void     osm_io_log(unsigned io, osm_cmd_ctx_t * ctx);
void     osm_ios_log(osm_cmd_ctx_t * ctx);

struct osm_cmd_link_t* osm_ios_add_commands(struct osm_cmd_link_t* tail);

void     osm_ios_inf_init(osm_measurements_inf_t* inf);
void     osm_ios_measurements_init(void);
