#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "base_types.h"
#include "io.h"


extern const unsigned ios_watch_ios[IOS_WATCH_COUNT];

bool io_watch_enable(unsigned io, bool enabled, io_pupd_t pupd);
void io_watch_isr(uint32_t exti_group);
void io_watch_init(void);
