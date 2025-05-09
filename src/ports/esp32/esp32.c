#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <driver/i2c.h>
#include <driver/gpio.h>
#include <soc/clk_tree_defs.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>

#include <osm/core/platform.h>

#include "pinmap.h"
#include <osm/core/log.h>
#include "platform_model.h"
#include <osm/comms/comms.h>
#include <osm/core/sleep.h>

#include <osm/core/adcs.h>

static bool _led_state = false;

static uint8_t _mac[6];

uint32_t platform_get_frequency(void)
{
    return SOC_CLK_XTAL32K_FREQ_APPROX;
}


const uint8_t * esp_port_get_mac(void)
{
    if (!_mac[0])
        ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_MODE_STA, _mac));
    return _mac;
}


uint32_t platform_get_hw_id(void)
{
    const uint8_t * mac = esp_port_get_mac();
    return (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5];
}


/* There is an echo on RS485 to remove. */
extern bool modbus_requires_echo_removal(void)
{
    return true;
}


void platform_blink_led_init(void)
{
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
   _led_state = false;
}


void platform_blink_led_toggle(void)
{
    _led_state = !_led_state;
    gpio_set_level(LED_PIN, (_led_state)?0:1);
}


void platform_watchdog_reset(void)
{
    esp_task_wdt_reset();
    vTaskDelay(1); /* Give IDLE time to kick watchdog too. */
}


void platform_watchdog_init(uint32_t ms)
{
}


void platform_init(void)
{
    gpio_config_t de_485_conf = {
        .pin_bit_mask = BIT64(DE_485_PIN),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&de_485_conf);
}


void platform_set_rs485_mode(bool driver_enable)
{
    gpio_set_direction(SW_SEL, GPIO_MODE_OUTPUT);
    gpio_set_level(SW_SEL, 1);
    if (driver_enable)
    {
        modbus_debug("driver:enable");
        gpio_set_level(DE_485_PIN, 1);
    }
    else
    {
        modbus_debug("driver:disable");
        gpio_set_level(DE_485_PIN, 0);
    }
}


void platform_main_loop_iterate(void)
{
    model_main_loop_iterate();
}


void platform_reset_sys(void)
{
    esp_restart();
}


bool platform_persist_commit(persist_storage_t* persist_data, persist_measurements_storage_t* persist_measurements)
{
    return false;
}


bool platform_overwrite_fw_page(uintptr_t dst, unsigned abs_page, uint8_t* fw_page)
{
    return false;
}


void platform_clear_flash_flags(void)
{
}


void platform_start(void)
{
    ;
}


bool platform_running(void)
{
    return true;
}


void platform_deinit(void)
{
}


void platform_setup_adc(adc_setup_config_t* config)
{
}


void platform_adc_set_regular_sequence(uint8_t num_adcs_types, adcs_type_t* adcs_types)
{
}


void platform_adc_start_conversion_regular(void)
{
}


void platform_adc_power_off(void)
{
}


void platform_adc_set_num_data(unsigned num_data)
{
}


void platform_hpm_enable(bool enable)
{
}


void platform_tight_loop(void) { }


uint32_t get_since_boot_ms(void)
{
    return pdTICKS_TO_MS(xTaskGetTickCount());
}


void platform_gpio_init(const port_n_pins_t * gpio_pin)
{
}


void platform_gpio_setup(const port_n_pins_t * gpio_pin, bool is_input, uint32_t pull)
{
    gpio_set_direction(*gpio_pin, (is_input)?GPIO_MODE_INPUT:GPIO_MODE_OUTPUT);

    gpio_pull_mode_t pull_mode = GPIO_FLOATING;
    switch(pull)
    {
        case GPIO_PUPD_PULLUP: pull_mode = GPIO_PULLUP_ONLY; break;
        case GPIO_PUPD_PULLDOWN: pull_mode = GPIO_PULLDOWN_ONLY; break;
        default: break;
    }

    gpio_set_pull_mode(*gpio_pin, pull_mode);
}

void platform_gpio_set(const port_n_pins_t * gpio_pin, bool is_on)
{
    gpio_set_level(*gpio_pin, (int)is_on);
}

bool platform_gpio_get(const port_n_pins_t * gpio_pin)
{
    return (bool)gpio_get_level(*gpio_pin);
}
