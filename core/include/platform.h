#pragma once


#include <stdint.h>


void platform_init(void);
void platform_watchdog_init(uint32_t ms);
void platform_watchdog_reset(void);
