#pragma once
#include <stdbool.h>

#include "base_types.h"
#include "measurements.h"
#include "io.h"


extern void     pulsecount_init(void);

extern void     pulsecount_enable(unsigned io, bool enable, io_pupd_t pupd, io_special_t edge);

extern void     pulsecount_inf_init(measurements_inf_t* inf);

struct cmd_link_t* pulsecount_add_commands(struct cmd_link_t* tail);

extern void     pulsecount_isr(uint32_t exti_group);
