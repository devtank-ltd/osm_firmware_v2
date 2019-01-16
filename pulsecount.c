#include <inttypes.h>
#include <stdlib.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "adcs.h"

static volatile unsigned pulsecount_A_value = 0;
static volatile unsigned pulsecount_A_psp = 0;

static volatile unsigned pulsecount_B_value = 0;
static volatile unsigned pulsecount_B_psp = 0;

#define PSP_ADC_INDEX 5


void exti2_3_isr()
{
    exti_reset_request(EXTI3);
    pulsecount_B_value++;
}


void exti4_15_isr()
{
    exti_reset_request(EXTI7);
    pulsecount_A_value++;
}


void pulsecount_do_samples()
{}



void pulsecount_second_boardary()
{
    pulsecount_A_psp   = pulsecount_A_value;
    pulsecount_B_psp   = pulsecount_B_value;
    pulsecount_B_value = pulsecount_A_value = 0;
}


void pulsecount_A_get(unsigned * pps, unsigned * min_v, unsigned * max_v)
{
    *pps = pulsecount_A_psp;
    adcs_get(PSP_ADC_INDEX, min_v, max_v, NULL);
}


void pulsecount_B_get(unsigned * pps)
{
    *pps = pulsecount_B_psp;
}


void pulsecount_init()
{
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_SYSCFG_COMP);

    gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO7);
    gpio_mode_setup(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO3);

    exti_select_source(EXTI7, GPIOC);
    exti_select_source(EXTI3, GPIOB);

    exti_set_trigger(EXTI7, EXTI_TRIGGER_FALLING);
    exti_set_trigger(EXTI3, EXTI_TRIGGER_FALLING);

    exti_enable_request(EXTI7);
    exti_enable_request(EXTI3);

    nvic_set_priority(NVIC_EXTI4_15_IRQ, 2);
    nvic_enable_irq(NVIC_EXTI4_15_IRQ);

    nvic_set_priority(NVIC_EXTI2_3_IRQ, 2);
    nvic_enable_irq(NVIC_EXTI2_3_IRQ);
}
