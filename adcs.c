#include <inttypes.h>
#include <stddef.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"

/* On some versions of gcc this header isn't defining it. Quick fix. */
#ifndef PRIu64
#define PRIu64 "llu"
#endif

static uint8_t adc_channel_array[] = ADC_CHANNELS;

char adc_temp_buffer[24];

typedef struct
{
    unsigned max_value;
    unsigned min_value;
    uint64_t total_value;
    unsigned count;
} adc_channel_info_t;


static volatile adc_channel_info_t adc_channel_info[ARRAY_SIZE(adc_channel_array)] = {{0}};

static volatile adc_channel_info_t adc_channel_info_cur[ARRAY_SIZE(adc_channel_array)] = {{0}};

static volatile uint16_t last_value[ARRAY_SIZE(adc_channel_array)] = {0};

static unsigned call_count = 0;
static unsigned adc_index = ARRAY_SIZE(adc_channel_array) - 1;

static basic_fixed_t scale, offset, adc_value, temp;


void adcs_init()
{
    const port_n_pins_t port_n_pins[] = ADCS_PORT_N_PINS;

    for(unsigned n = 0; n < ARRAY_SIZE(port_n_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(port_n_pins[n].port));
        gpio_mode_setup(port_n_pins[n].port,
                        GPIO_MODE_ANALOG,
                        GPIO_PUPD_NONE,
                        port_n_pins[n].pins);
    }

    rcc_peripheral_enable_clock(&RCC_APB2ENR, RCC_APB2ENR_ADCEN);

    adc_power_off(ADC1);
    adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
    adc_calibrate(ADC1);
    adc_set_operation_mode(ADC1, ADC_MODE_SEQUENTIAL);
    adc_set_continuous_conversion_mode(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_regular_sequence(ADC1, ARRAY_SIZE(adc_channel_array), adc_channel_array);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_013DOT5);
    adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
    adc_power_on(ADC1);
}


void adcs_do_samples()
{
    call_count++;

    unsigned state = call_count % 2;

    switch(state)
    {
        case 0: adc_start_conversion_regular(ADC1); break;
        case 1:
        {
            if (adc_eoc(ADC1))
            {
                uint32_t adc = adc_read_regular(ADC1);

                volatile adc_channel_info_t * channel_info = &adc_channel_info[adc_index];

                if (adc > channel_info->max_value)
                    channel_info->max_value = adc;

                if (adc < channel_info->min_value)
                    channel_info->min_value = adc;

                channel_info->total_value += adc;
                channel_info->count ++;
                last_value[adc_index]   = adc;

            }
            else log_debug(DEBUG_ADC, "ADC sampling not complete!");
                adc_index = (adc_index + 1) % ARRAY_SIZE(adc_channel_array);
        }
    }
}


void adcs_collect_boardary()
{
    unsigned sample_count = call_count / ARRAY_SIZE(adc_channel_array) / 2;

    log_debug(DEBUG_ADC, "ADCS SPS %u", sample_count);

    for(unsigned n = 0; n < ARRAY_SIZE(adc_channel_array); n++)
    {
        volatile adc_channel_info_t * channel_info = &adc_channel_info[n];

        adc_channel_info_cur[n] = *channel_info;

        channel_info->max_value = 0;
        channel_info->min_value = 0xFFFFFFFF;
        channel_info->total_value  = 0;
        channel_info->count = 0;

        if (adc_channel_info_cur[n].count != sample_count)
            log_debug(DEBUG_ADC, "ADC %u %u != %u", n, adc_channel_info_cur[n].count, sample_count);
    }

    call_count = 0;
}


unsigned adcs_get_count()
{
    return ARRAY_SIZE(adc_channel_array);
}


unsigned  adcs_get_last(unsigned adc)
{
    if (adc >= ARRAY_SIZE(adc_channel_array))
        return 0;

    return last_value[adc];
}


unsigned  adcs_get_tick(unsigned adc)
{
    if (adc >= ARRAY_SIZE(adc_channel_array))
        return 0;

    return adc_channel_info[adc].count;
}


static bool _process_poly(basic_fixed_t * cals, unsigned count)
{
    /*
     * Polynomial calibration is an array of constants.
     *
     * f(x) = -0.308909161295055 x³ + 11.4453457522623 x² + 38.6754604611234 x + 313.076878223555
     * A = -0.30890916129505
     * B = 11.4453457522623
     * C = 38.6754604611234
     * D = 313.076878223555
     * f(x) = Ax³ + Bx² + Cx + D
     * */
    basic_fixed_t result = {.raw = 0};
    for(unsigned n = 0; n < count ; n++)
    {
        unsigned pwr = count-1-n;

        if (pwr)
        {
            temp.raw = adc_value.raw;
            pwr--;
            while(pwr--)
                if (!basic_fixed_mul(&temp, &temp, &adc_value))
                    return false;

            if (!basic_fixed_mul(&temp, &temp, &cals[n]))
                return false;

            if (!basic_fixed_add(&result, &result, &temp))
                return false;
        }
        else
        {
            if (!basic_fixed_add(&result, &result, &cals[n]))
                return false;
        }
    }

    adc_value.raw = result.raw;
    return true;
}


void adcs_adc_log(unsigned adc)
{
    if (adc >= ARRAY_SIZE(adc_channel_array))
        return;

    volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[adc];

    log_out("ADC : %u", adc);

    cal_type_t type;

    if (persistent_get_cal_type(adc, &type) && type != ADC_NO_CAL)
    {
        const char * unit = NULL;

        if (type == ADC_LIN_CAL)
        {
            if (persistent_get_cal(adc, &scale, &offset, &unit))
            {
                volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[adc];

                if (basic_fixed_set_whole(&adc_value, channel_info->min_value) &&
                    basic_fixed_mul(&adc_value, &adc_value, &scale) &&
                    basic_fixed_add(&adc_value, &adc_value, &offset) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Min : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Min : BAD MATH");

                if (basic_fixed_set_whole(&adc_value, channel_info->max_value) &&
                    basic_fixed_mul(&adc_value, &adc_value, &scale) &&
                    basic_fixed_add(&adc_value, &adc_value, &offset) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Max : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Max : BAD MATH");

                if (basic_fixed_set_whole(&adc_value, channel_info->total_value) &&
                    basic_fixed_set_whole(&temp, channel_info->count) &&
                    basic_fixed_div(&adc_value, &adc_value, &temp) &&
                    basic_fixed_mul(&adc_value, &adc_value, &scale) &&
                    basic_fixed_add(&adc_value, &adc_value, &offset) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Avg : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Avg : BAD MATH");
            }
            else
            {
                log_out("Min : BAD CAL");
                log_out("Max : BAD CAL");
                log_out("Avg : BAD CAL");
            }
        }
        else if (type == ADC_POLY_CAL)
        {
            basic_fixed_t * cals = NULL;
            unsigned count = 0;
            if (persistent_get_exp_cal(adc, &cals, &count, &unit))
            {
                volatile adc_channel_info_t * channel_info = &adc_channel_info_cur[adc];

                if (basic_fixed_set_whole(&adc_value, channel_info->min_value) &&
                    _process_poly(cals, count) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Min : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Min : BAD MATH");

                if (basic_fixed_set_whole(&adc_value, channel_info->max_value) &&
                    _process_poly(cals, count) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Max : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Max : BAD MATH");

                if (basic_fixed_set_whole(&adc_value, channel_info->total_value) &&
                    basic_fixed_set_whole(&temp, channel_info->count) &&
                    basic_fixed_div(&adc_value, &adc_value, &temp) &&
                    _process_poly(cals, count) &&
                    basic_fixed_to_str(&adc_value, adc_temp_buffer, sizeof(adc_temp_buffer)))
                    log_out("Avg : %s%s", adc_temp_buffer, unit);
                else
                    log_out("Avg : BAD MATH");
            }
            else
            {
                log_out("Min : BAD CAL");
                log_out("Max : BAD CAL");
                log_out("Avg : BAD CAL");
            }
        }
        else
        {
            log_out("Min : BAD CAL");
            log_out("Max : BAD CAL");
            log_out("Avg : BAD CAL");
        }
    }
    else
    {
        log_out("Min : %u", channel_info->min_value);
        log_out("Max : %u", channel_info->max_value);
        log_out("Avg : %"PRIu64" / %u", channel_info->total_value, channel_info->count);
    }
}


void adcs_log()
{
    for(unsigned n = 0; n < ARRAY_SIZE(adc_channel_info_cur); n++)
        adcs_adc_log(n);
}
