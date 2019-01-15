#include <inttypes.h>

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

#define PSP_ADC_INDEX 5

static uint8_t adc_channel_array[] = {4,6,7,8,9,10,11,12,13,14,15};

typedef struct
{
    unsigned max_value;
    unsigned min_value;
    double   av_value;
}  __attribute__((packed)) adc_channel_info_t;

static adc_channel_info_t adc_channel_info[ARRAY_SIZE(adc_channel_array)] = {{0}};

static volatile adc_channel_info_t adc_channel_info_cur[ARRAY_SIZE(adc_channel_array)] = {{0}};


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
    static unsigned count = 999;

    count++;

    if (count == 1000)
    {
        pulsecount_A_psp   = pulsecount_A_value;
        pulsecount_B_psp   = pulsecount_B_value;
        pulsecount_B_value = pulsecount_A_value = 0;
        count              = 0;

        for(unsigned n = 0; n < ARRAY_SIZE(adc_channel_array); n++)
        {
            adc_channel_info_t * channel_info = &adc_channel_info[n];

            adc_channel_info_cur[n] = *channel_info;

            channel_info->max_value = 0;
            channel_info->min_value = 0xFFFFFFFF;
            channel_info->av_value  = 0;

            adc_channel_info_cur[n].av_value /= 1000.0;
        }
    }


    if (!count)
        log_debug_raw(LOG_SPACER);

    for(unsigned n = 0; n < ARRAY_SIZE(adc_channel_array); n++)
    {
        adc_start_conversion_regular(ADC1);
        while (!(adc_eoc(ADC1)));

        uint32_t adc = adc_read_regular(ADC1);

        adc_channel_info_t * channel_info = &adc_channel_info[n];

        if (adc > channel_info->max_value)
            channel_info->max_value = adc;

        if (adc < channel_info->min_value)
            channel_info->min_value = adc;

        channel_info->av_value += adc;
    }

    timer_clear_flag(TIM3, TIM_SR_CC1IF);
}


void pulsecount_A_get(unsigned * pps, unsigned * min_v, unsigned * max_v)
{
    volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[PSP_ADC_INDEX];
    *pps   = pulsecount_A_psp;
    *min_v = channel_info->min_value;
    *max_v = channel_info->max_value;
}


void pulsecount_B_get(unsigned * pps)
{
    *pps = pulsecount_B_psp;
}


void adc_log()
{
    log_out(LOG_SPACER);
    for(unsigned n = 0; n < ARRAY_SIZE(adc_channel_info_cur); n++)
    {
        volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[n];

        log_out("Channel : %u", adc_channel_array[n]);
        log_out("Min     : %u", channel_info->min_value);
        log_out("Max     : %u", channel_info->max_value);
        log_out("Avg     : %f", channel_info->av_value);
    }
    log_out(LOG_SPACER);
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

    //ch4, 6, 7
    gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO4 | GPIO6 | GPIO7);

    //ch8, 9
    gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0 | GPIO1);

    //ch10,11,12,13,14,15
    gpio_mode_setup(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO0 | GPIO1 | GPIO2 | GPIO3 | GPIO4| GPIO5);

    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_ADCEN);

    adc_power_off(ADC1);
    adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
    adc_calibrate(ADC1);
    adc_set_operation_mode(ADC1, ADC_MODE_SEQUENTIAL);
    adc_set_continuous_conversion_mode(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_regular_sequence(ADC1, ARRAY_SIZE(adc_channel_array), adc_channel_array);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_001DOT5);
    adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
    adc_power_on(ADC1);
}
