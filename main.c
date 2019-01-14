#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/timer.h>


#include "cmd.h"
#include "log.h"
#include "usb.h"
#include "uarts.h"




void hard_fault_handler(void)
{
    platform_raw_msg("----big fat libopen3 crash -----");
    while(true);
}


static unsigned counter = 0;

void sys_tick_handler(void)
{
    usb_iterate();
    if (counter == 1000)
    {
        gpio_toggle(GPIOA, GPIO5);
        counter = 0;
    }
    counter++;
}


int main(void)
{
    rcc_clock_setup_pll(&rcc_hse8mhz_configs[RCC_CLOCK_HSE8_72MHZ]);
    uarts_setup();

    platform_raw_msg("----start----");

    log_init();
    cmds_init();
    usb_init();

    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    gpio_clear(GPIOA, GPIO5);

    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    systick_set_reload(rcc_ahb_frequency / 8 / 1000);
    systick_counter_enable();
    systick_interrupt_enable();

    while(true)
        cmds_process();

    return 0;
}
