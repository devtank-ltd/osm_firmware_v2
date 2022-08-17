#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/iwdg.h>

#include "platform.h"

#include "sos.h"
#include "pinmap.h"
#include "log.h"


static void _stm_setup_blink_led(void)
{
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);
}


static void _stm_setup_systick(void)
{
    systick_set_frequency(1000, rcc_ahb_frequency);
    systick_counter_enable();
    systick_interrupt_enable();
}


// cppcheck-suppress unusedFunction ; System handler
void hard_fault_handler(void)
{
    /* Special libopencm3 function to handle crashes */
    platform_raw_msg("----big fat crash -----");
    error_state();
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
    _stm_setup_systick();
    _stm_setup_blink_led();
}
