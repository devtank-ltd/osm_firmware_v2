#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>
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


void tim3_isr(void)
{
    cmds_process();
    usb_iterate();
}


int main(void)
{
    rcc_clock_setup_pll(&rcc_hse8mhz_configs[RCC_CLOCK_HSE8_72MHZ]);
    uarts_setup();

    platform_raw_msg("----start----");

    log_init();
    cmds_init();
    usb_init();

    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);
    timer_continuous_mode(TIM3);

    // 1000Hz
    timer_set_prescaler(TIM3, 72);
    timer_set_period(TIM3, 1);

	nvic_enable_irq(NVIC_TIM3_IRQ);
	timer_enable_update_event(TIM3);
	timer_enable_irq(TIM3, TIM_DIER_UIE);
    nvic_set_priority(NVIC_TIM3_IRQ, 0);
    timer_enable_counter(TIM3);

    while(true);

    return 0;
}
