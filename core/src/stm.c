#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/scb.h>

#include "platform.h"

#include "sos.h"
#include "pinmap.h"
#include "log.h"


static void _stm_setup_systick(void)
{
    systick_set_frequency(1000, rcc_ahb_frequency);
    systick_counter_enable();
    systick_interrupt_enable();
}

static void _stm_setup_rs485(void)
{
    port_n_pins_t re_port_n_pin = RE_485_PIN;
    port_n_pins_t de_port_n_pin = DE_485_PIN;

    rcc_periph_clock_enable(PORT_TO_RCC(re_port_n_pin.port));
    rcc_periph_clock_enable(PORT_TO_RCC(de_port_n_pin.port));

    gpio_mode_setup(re_port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, re_port_n_pin.pins);
    gpio_mode_setup(de_port_n_pin.port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, de_port_n_pin.pins);

    platform_set_rs485_mode(false);
}


// cppcheck-suppress unusedFunction ; System handler
void hard_fault_handler(void)
{
    /* Special libopencm3 function to handle crashes */
    platform_raw_msg("----big fat crash -----");
    error_state();
}


void platform_blink_led_init(void)
{
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);
}


void platform_blink_led_toggle(void)
{
    gpio_toggle(LED_PORT, LED_PIN);
}


void platform_watchdog_reset(void)
{
    iwdg_reset();
}


void platform_watchdog_init(uint32_t ms)
{
    iwdg_set_period_ms(ms);
    iwdg_start();
}


void platform_init(void)
{
    /* Main clocks setup in bootloader, but of course libopencm3 doesn't know. */
    rcc_ahb_frequency = 80e6;
    rcc_apb1_frequency = 80e6;
    rcc_apb2_frequency = 80e6;

    _stm_setup_systick();
    _stm_setup_rs485();
}


void platform_set_rs485_mode(bool driver_enable)
{
    /* ST3485E
     *
     * 2. RE Receiver output enable. RO is enabled when RE is low; RO is
     * high impedance when RE is high. If RE is high and DE is low, the
     * device will enter a low power shutdown mode.

     * 3. DE Driver output enable. The driver outputs are enabled by
     * bringing DE high. They are high impedance when DE is low. If RE
     * is high DE is low, the device will enter a low-power shutdown
     * mode. If the driver outputs are enabled, the part functions as
     * line driver, while they are high impedance, it functions as line
     * receivers if RE is low.
     *
     * */

    port_n_pins_t re_port_n_pin = RE_485_PIN;
    port_n_pins_t de_port_n_pin = DE_485_PIN;
    if (driver_enable)
    {
        modbus_debug("driver:enable receiver:disable");
        gpio_set(re_port_n_pin.port, re_port_n_pin.pins);
        gpio_set(de_port_n_pin.port, de_port_n_pin.pins);
    }
    else
    {
        modbus_debug("driver:disable receiver:enable");
        gpio_clear(re_port_n_pin.port, re_port_n_pin.pins);
        gpio_clear(de_port_n_pin.port, de_port_n_pin.pins);
    }
}


void platform_reset_sys(void)
{
    scb_reset_system();
}


persist_storage_t* platform_get_raw_persist(void)
{
    return (persist_storage_t*)PERSIST_RAW_DATA;
}


void platform_persist_commit(void)
{
    flash_unlock();
    flash_erase_page(FLASH_CONFIG_PAGE);
    flash_set_data(PERSIST_RAW_DATA, &persist_data, sizeof(persist_data));
    flash_lock();
}

void platform_persist_wipe(void)
{
    flash_unlock();
    flash_erase_page(FLASH_CONFIG_PAGE);
    flash_lock();
}
