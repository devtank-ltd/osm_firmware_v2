#include <inttypes.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "pinmap.h"

static volatile unsigned pulsecount_1_value = 0;
static volatile unsigned pulsecount_1_psp = 0;

static volatile unsigned pulsecount_2_value = 0;
static volatile unsigned pulsecount_2_psp = 0;


void PPS1_EXTI2_3_ISR()
{
    exti_reset_request(PPS1_EXTI3);
    pulsecount_2_value++;
}


void PPS2_EXTI4_15_ISR()
{
    exti_reset_request(PPS2_EXTI7);
    pulsecount_1_value++;
}


void pulsecount_do_samples()
{}



void pulsecount_second_boardary()
{
    pulsecount_1_psp   = pulsecount_1_value;
    pulsecount_2_psp   = pulsecount_2_value;
    pulsecount_2_value = pulsecount_1_value = 0;
}


void pulsecount_1_get(unsigned * pps)
{
    *pps = pulsecount_1_psp;
}


void pulsecount_2_get(unsigned * pps)
{
    *pps = pulsecount_2_psp;
}


void pulsecount_init()
{
    rcc_periph_clock_enable(PPS1_GPIOB_RCC);
    rcc_periph_clock_enable(PPS2_GPIOC_RCC);
    rcc_periph_clock_enable(RCC_SYSCFG_COMP);

    gpio_mode_setup(PPS2_GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, PPS2_GPIO7);
    gpio_mode_setup(PPS1_GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, PPS1_GPIO3);

    exti_select_source(PPS2_EXTI7, PPS2_GPIOC);
    exti_select_source(PPS1_EXTI3, PPS1_GPIOB);

    exti_set_trigger(PPS2_EXTI7, EXTI_TRIGGER_FALLING);
    exti_set_trigger(PPS1_EXTI3, EXTI_TRIGGER_FALLING);

    exti_enable_request(PPS2_EXTI7);
    exti_enable_request(PPS1_EXTI3);

    nvic_set_priority(PPS2_NVIC_EXTI4_15_IRQ, 2);
    nvic_enable_irq(PPS2_NVIC_EXTI4_15_IRQ);

    nvic_set_priority(PPS1_NVIC_EXTI2_3_IRQ, 2);
    nvic_enable_irq(PPS1_NVIC_EXTI2_3_IRQ);
}
