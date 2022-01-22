#include <stdint.h>
#include <inttypes.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>

#include "pinmap.h"
#include "cmd.h"
#include "log.h"
#include "uarts.h"
#include "adcs.h"
#include "pulsecount.h"
#include "timers.h"
#include "io.h"
#include "sai.h"
#include "uart_rings.h"
#include "persist_config.h"
#include "lorawan.h"
#include "measurements.h"
#include "sys_time.h"
#include "modbus.h"
#include "sos.h"
#include "timers.h"
#include "htu21d.h"
#include "i2c.h"

volatile uint32_t since_boot_ms = 0;


void hard_fault_handler(void)
{
    platform_raw_msg("----big fat crash -----");
    error_state();
}


uint32_t since_boot_delta(uint32_t newer, uint32_t older)
{
    if (newer == older)
        return 0;

    if (newer > older)
        return newer - older;
    else
    {
        return (0xFFFFFFFF - older) + newer;
    }
}


void sys_tick_handler(void)
{
    since_boot_ms++;
}


int main(void)
{
    /* Main clocks setup in bootloader, but of course libopencm3 doesn't know. */
    rcc_ahb_frequency = 80e6;
    rcc_apb1_frequency = 80e6;
    rcc_apb2_frequency = 80e6;

    uarts_setup();
    uart_rings_init();

    systick_set_frequency(1000, rcc_ahb_frequency);
    systick_counter_enable();
    systick_interrupt_enable();

    platform_raw_msg("----start----");
    log_sys_debug("Frequency : %lu", rcc_ahb_frequency);
    log_sys_debug("Version : %s", GIT_VERSION);

    init_persistent();
    timers_init();
    log_init();
    cmds_init();
    ios_init();
    sai_init();
    adcs_init();
    htu21d_init();
    lorawan_init();
    measurements_init();
    modbus_init();

    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    gpio_clear(LED_PORT, LED_PIN);

    log_async_log = true;

    uint32_t prev_now = 0;
    while(true)
    {
        while(since_boot_delta(since_boot_ms, prev_now) < 1000)
        {
            uart_rings_in_drain();
            uart_rings_out_drain();
            measurements_loop_iteration();
        }

        gpio_toggle(LED_PORT, LED_PIN);

        prev_now = since_boot_ms;
    }

    return 0;
}
