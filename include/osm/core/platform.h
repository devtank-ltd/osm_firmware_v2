#pragma once


#include <stdint.h>
#include <stdbool.h>

#include <osm/core/persist_config_header.h>


uint32_t osm_platform_get_frequency(void);
uint32_t osm_platform_get_hw_id(void);

void osm_platform_init(void);
void osm_platform_start(void);
void osm_platform_watchdog_init(uint32_t ms);
void osm_platform_watchdog_reset(void);
void osm_platform_blink_led_init(void);
void osm_platform_blink_led_toggle(void);
void osm_platform_set_rs485_mode(bool driver_enable);
void osm_platform_reset_sys(void);
void osm_platform_hard_reset_sys(void);
osm_persist_storage_t* osm_platform_get_raw_persist(void);
osm_persist_measurements_storage_t* osm_platform_get_measurements_raw_persist(void);
bool osm_platform_persist_commit(osm_persist_storage_t * persist_data, osm_persist_measurements_storage_t* persist_measurements);
void osm_platform_persist_wipe(void);
bool osm_platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page);
uintptr_t osm_platform_get_fw_addr(unsigned fw_page_index);
void osm_platform_finish_fw(void);
void osm_platform_clear_flash_flags(void);

bool osm_platform_running(void);
void osm_platform_deinit(void);

void osm_platform_hpm_enable(bool enable);

void osm_platform_tight_loop(void);
void osm_platform_main_loop_iterate(void);

void osm_platform_gpio_init(const osm_port_n_pins_t * gpio_pin);
void osm_platform_gpio_setup(const osm_port_n_pins_t * gpio_pin, bool is_input, uint32_t pull);
void osm_platform_gpio_set(const osm_port_n_pins_t * gpio_pin, bool is_on);
bool osm_platform_gpio_get(const osm_port_n_pins_t * gpio_pin);

