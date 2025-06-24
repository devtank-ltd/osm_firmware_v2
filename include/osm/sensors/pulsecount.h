#pragma once
#include <stdbool.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/core/io.h>


void     osm_pulsecount_init(void);

void     osm_pulsecount_enable(unsigned io, bool enable, osm_io_pupd_t pupd, osm_io_special_t edge);

void     osm_pulsecount_inf_init(osm_measurements_inf_t* inf);

struct osm_cmd_link_t* osm_pulsecount_add_commands(struct osm_cmd_link_t* tail);

void     osm_pulsecount_isr(uint32_t exti_group);
