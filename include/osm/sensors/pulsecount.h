#pragma once
#include <stdbool.h>

#include <osm/core/base_types.h>
#include <osm/core/measurements.h>
#include <osm/core/io.h>


void     pulsecount_init(void);

void     pulsecount_enable(unsigned io, bool enable, io_pupd_t pupd, io_special_t edge);

void     pulsecount_inf_init(measurements_inf_t* inf);

struct cmd_link_t* pulsecount_add_commands(struct cmd_link_t* tail);

void     pulsecount_isr(uint32_t exti_group);
