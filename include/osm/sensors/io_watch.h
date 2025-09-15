#pragma once
#include <stdbool.h>
#include <stdint.h>

#include <osm/core/base_types.h>
#include <osm/core/io.h>


extern const unsigned ios_watch_ios[IOS_WATCH_COUNT];

bool osm_io_watch_enable(unsigned io, bool enabled, osm_io_pupd_t pupd);
void osm_io_watch_isr(uint32_t exti_group);
void osm_io_watch_init(void);
