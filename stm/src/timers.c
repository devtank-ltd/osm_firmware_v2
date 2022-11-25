#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>

#include "config.h"
#include "timers.h"
#include "log.h"


static volatile uint32_t us_counter = 0;


void timer_delay_us(uint16_t wait_us)
{
    TIM_ARR(TIM2) = wait_us;
    TIM_EGR(TIM2) = TIM_EGR_UG;
    //TIM_CR1(TIM2) |= TIM_CR1_CEN;
    timer_enable_counter(TIM2);
    while (TIM_CR1(TIM2) & TIM_CR1_CEN);
}


void timer_delay_us_64(uint64_t wait_us)
{
    while (wait_us > UINT16_MAX)
    {
        timer_delay_us(UINT16_MAX);
        wait_us -= UINT16_MAX;
    }
    timer_delay_us(wait_us);
}


void     timers_init()
{
    rcc_periph_clock_enable(RCC_TIM2);

    timer_disable_counter(TIM2);

    timer_set_mode(TIM2,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(TIM2, rcc_ahb_frequency / 1000000-1);
    timer_set_period(TIM2, 0xffff);
    timer_enable_preload(TIM2);
    timer_one_shot_mode(TIM2);
}
