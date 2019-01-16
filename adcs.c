#include <inttypes.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"


static uint8_t adc_channel_array[] = {4,6,7,8,9,10,11,12,13,14,15};

typedef struct
{
    unsigned max_value;
    unsigned min_value;
    double   av_value;
}  __attribute__((packed)) adc_channel_info_t;

static adc_channel_info_t adc_channel_info[ARRAY_SIZE(adc_channel_array)] = {{0}};

static volatile adc_channel_info_t adc_channel_info_cur[ARRAY_SIZE(adc_channel_array)] = {{0}};


void adcs_init()
{
    //ch4, 6, 7
    gpio_mode_setup(ADCS_GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADCS_A_GPIO4_GPIO6_GPIO7);

    //ch8, 9
    gpio_mode_setup(ADCS_GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADCS_B_GPIO0_GPIO1);

    //ch10,11,12,13,14,15
    gpio_mode_setup(ADCS_GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADCS_C_GPIO0_GPIO1_GPIO2_GPIO3_GPIO4_GPIO5);

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


void adcs_do_samples()
{
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
}


void adcs_second_boardary()
{
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


void adcs_get(unsigned   adc,
              unsigned * min_value,
              unsigned * max_value,
              double *   av_value)
{
    if (adc >= ARRAY_SIZE(adc_channel_array))
    {
        if (max_value)
            *max_value = 0;
        if (min_value)
            *min_value = 0;
        if (av_value)
            *av_value = 0;
        return;
    }

    volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[adc];

    if (max_value)
        *max_value = channel_info->max_value;
    if (min_value)
        *min_value = channel_info->min_value;
    if (av_value)
        *av_value = channel_info->av_value;
}


unsigned adcs_get_count()
{
    return ARRAY_SIZE(adc_channel_array);
}


void adcs_log()
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
