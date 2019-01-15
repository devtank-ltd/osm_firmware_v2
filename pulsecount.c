#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"

static volatile unsigned pulsecount_value = 0;
static volatile unsigned pulsecount_psp = 0;


void exti4_15_isr()
{
    exti_reset_request(EXTI7);
    pulsecount_value++;
}


void tim3_isr(void)
{
    pulsecount_psp = pulsecount_value;
    pulsecount_value = 0;

    timer_clear_flag(TIM3, TIM_SR_CC1IF);
}


unsigned pulsecount_get()
{
    return pulsecount_psp;
}



void pulsecount_init()
{
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_SYSCFG_COMP);

    gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO7);

    exti_select_source(EXTI7, GPIOC);

    exti_set_trigger(EXTI7, EXTI_TRIGGER_FALLING);

    exti_enable_request(EXTI7);

    nvic_set_priority(NVIC_EXTI4_15_IRQ, 2);
    nvic_enable_irq(NVIC_EXTI4_15_IRQ);

    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);

    timer_set_mode(TIM3,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow 
    timer_set_prescaler(TIM3, rcc_ahb_frequency/1000-1);
    timer_set_period(TIM3, 1000-1);

    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);

    timer_enable_counter(TIM3);
    timer_enable_irq(TIM3, TIM_DIER_CC1IE);

    timer_set_counter(TIM3, 0);
    nvic_enable_irq(NVIC_TIM3_IRQ);
    nvic_set_priority(NVIC_TIM3_IRQ, 1);
}
