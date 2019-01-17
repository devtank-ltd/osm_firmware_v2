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


void PPS1_EXTI_ISR()
{
    exti_reset_request(PPS1_EXTI);
    pulsecount_2_value++;
}


void PPS2_EXTI_ISR()
{
    exti_reset_request(PPS2_EXTI);
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
    rcc_periph_clock_enable(PORT_TO_RCC(PPS1_PORT));
    rcc_periph_clock_enable(PORT_TO_RCC(PPS2_PORT));

    rcc_periph_clock_enable(RCC_SYSCFG_COMP);

    gpio_mode_setup(PPS1_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, PPS1_PINS);
    gpio_mode_setup(PPS2_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, PPS2_PINS);

    exti_select_source(PPS1_EXTI, PPS1_PORT);
    exti_select_source(PPS2_EXTI, PPS2_PORT);

    exti_set_trigger(PPS1_EXTI, EXTI_TRIGGER_FALLING);
    exti_set_trigger(PPS2_EXTI, EXTI_TRIGGER_FALLING);

    exti_enable_request(PPS1_EXTI);
    exti_enable_request(PPS2_EXTI);

    nvic_set_priority(PPS1_NVIC_EXTI_IRQ, 2);
    nvic_enable_irq(PPS1_NVIC_EXTI_IRQ);

    nvic_set_priority(PPS2_NVIC_EXTI_IRQ, 2);
    nvic_enable_irq(PPS2_NVIC_EXTI_IRQ);
}
