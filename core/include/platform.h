#pragma once


#include <stdint.h>

extern uint32_t rcc_ahb_frequency;
extern uint32_t rcc_apb1_frequency;
extern uint32_t rcc_apb2_frequency;

void platform_init(void);
void platform_watchdog_init(uint32_t ms);
void platform_watchdog_reset(void);
void platform_blink_led_init(void);
void platform_blink_led_toggle(void);

