#pragma once


#include <stdint.h>
#include <stdbool.h>

#include "persist_config_header.h"


uint32_t platform_get_frequency(void);

void platform_init(void);
void platform_start(void);
void platform_watchdog_init(uint32_t ms);
void platform_watchdog_reset(void);
void platform_blink_led_init(void);
void platform_blink_led_toggle(void);
void platform_set_rs485_mode(bool driver_enable);
void platform_reset_sys(void);
persist_storage_t* platform_get_raw_persist(void);
persist_measurements_storage_t* platform_get_measurements_raw_persist(void);
bool platform_persist_commit(persist_storage_t * persist_data, persist_measurements_storage_t* persist_measurements);
void platform_persist_wipe(void);
bool platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page);
void platform_clear_flash_flags(void);

bool platform_running(void);
void platform_deinit(void);

void platform_setup_adc(adc_setup_config_t* config);
void platform_adc_set_regular_sequence(uint8_t num_channels, adcs_type_t* channels);
void platform_adc_start_conversion_regular(void);
void platform_adc_power_off(void);
void platform_adc_set_num_data(unsigned num_data);

void platform_hpm_enable(bool enable);

void platform_tight_loop(void);

void platform_gpio_init(const port_n_pins_t * gpio_pin);
void platform_gpio_setup(const port_n_pins_t * gpio_pin, bool is_input, uint32_t pull);
void platform_gpio_set(const port_n_pins_t * gpio_pin, bool is_on);
bool platform_gpio_get(const port_n_pins_t * gpio_pin);

