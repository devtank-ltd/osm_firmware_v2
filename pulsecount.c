#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"

static volatile unsigned pulsecount_A_value = 0;
static volatile unsigned pulsecount_A_psp = 0;

static volatile unsigned pulsecount_B_value = 0;
static volatile unsigned pulsecount_B_psp = 0;

static volatile uint32_t adc_max = 0;
static volatile uint32_t adc_min = 0xFFFFFFFF;

static volatile unsigned cur_adc_max = 0;
static volatile unsigned cur_adc_min = 0xFFFFFFFF;


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


void tim3_isr(void)
{
    timer_clear_flag(TIM3, TIM_SR_CC1IF);

    static unsigned count = 999;

    count++;

    if (count == 1000)
    {
        pulsecount_A_psp = pulsecount_A_value;
        pulsecount_B_psp = pulsecount_B_value;
        pulsecount_B_value = pulsecount_A_value = 0;
        count = 0;
        cur_adc_max = adc_max;
        cur_adc_min = adc_min;
        adc_max = 0;
        adc_min = 0xFFFFFFFF;
    }

    adc_start_conversion_regular(ADC1);
    while (!(adc_eoc(ADC1)));
    uint32_t adc = adc_read_regular(ADC1);


    if (adc > adc_max)
        adc_max = adc;

    if (adc < adc_min)
        adc_min = adc;
}


void pulsecount_A_get(unsigned * pps, unsigned * min_v, unsigned * max_v)
{
    *pps   = pulsecount_A_psp;
    *min_v = (unsigned)cur_adc_min;
    *max_v = (unsigned)cur_adc_max;
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

    rcc_periph_clock_enable(RCC_TIM3);

    timer_disable_counter(TIM3);

    timer_set_mode(TIM3,
                   TIM_CR1_CKD_CK_INT,
                   TIM_CR1_CMS_EDGE,
                   TIM_CR1_DIR_UP);
    //-1 because it starts at zero, and interrupts on the overflow
    timer_set_prescaler(TIM3, rcc_ahb_frequency / 1000000-1);
    timer_set_period(TIM3, 1000-1);

    timer_enable_preload(TIM3);
    timer_continuous_mode(TIM3);

    timer_enable_counter(TIM3);
    timer_enable_irq(TIM3, TIM_DIER_CC1IE);

    timer_set_counter(TIM3, 0);
    nvic_enable_irq(NVIC_TIM3_IRQ);
    nvic_set_priority(NVIC_TIM3_IRQ, 1);

    gpio_mode_setup(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0);

    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_ADCEN);

    adc_power_off(ADC1);
    adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
    adc_calibrate(ADC1);
    adc_set_operation_mode(ADC1, ADC_MODE_SEQUENTIAL);
    adc_set_continuous_conversion_mode(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_239DOT5);

    uint8_t channel_array[] = {10};
    adc_set_regular_sequence(ADC1, ARRAY_SIZE(channel_array), channel_array);
    adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
    adc_power_on(ADC1);
}
