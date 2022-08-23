#pragma once


#include <stdint.h>
#include <stdbool.h>

#include "persist_config_header.h"


extern uint32_t rcc_ahb_frequency;
extern uint32_t rcc_apb1_frequency;
extern uint32_t rcc_apb2_frequency;

void platform_init(void);
void platform_watchdog_init(uint32_t ms);
void platform_watchdog_reset(void);
void platform_blink_led_init(void);
void platform_blink_led_toggle(void);
void platform_set_rs485_mode(bool driver_enable);
void platform_reset_sys(void);
persist_storage_t* platform_get_raw_persist(void);
bool platform_persist_commit(persist_storage_t * persist_data);
void platform_persist_wipe(void);

bool platform_running(void);
void platform_deinit(void);
