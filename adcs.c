#include <inttypes.h>
#include <stddef.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"


static uint8_t adc_channel_array[] = ADC_CHANNELS;


static void adcs_setup_adc(void)
{
    rcc_periph_clock_enable(RCC_ADC1);
    rcc_periph_clock_enable(RCC_GPIOA);

    gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO5);
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    adc_power_off(ADC1);
    if (ADC_CR(ADC1) && ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }
                                                                            // adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
    adc_calibrate(ADC1);                                                    // Exists but get held up
    adc_set_single_conversion_mode(ADC1);                                   // adc_set_operation_mode(ADC1, ADC_MODE_SCAN);
    adc_enable_regulator(ADC1);                                            // adc_disable_external_trigger_regular(ADC1);
    adc_set_right_aligned(ADC1);
    adc_enable_vrefint();
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_6DOT5CYC);     // adc_set_sample_time_on_all_channels(ADC1, ADC_SMPTIME_071DOT5);
    adc_set_regular_sequence(ADC1, 1, adc_channel_array);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);                         // adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT);
                                                                            // adc_disable_analog_watchdog(ADC1);
    adc_power_on(ADC1);

    /* Wait for ADC starting up. */
    for (int i = 0; i < 800000; i++)
    {    /* Wait a bit. */
        __asm__("nop");
    }
}


static uint16_t adcs_read(uint32_t adc, uint8_t channel)
{
    uint8_t local_channel_array[1] = { channel };
    adc_set_regular_sequence(adc, 1, local_channel_array);
    adc_start_conversion_regular(adc);
    while (!(adc_eoc(adc)));
    return adc_read_regular(adc);
}


void adcs_init(void)
{
    // Setup the clock and gpios
    const port_n_pins_t port_n_pins[] = ADCS_PORT_N_PINS;
    for(unsigned n = 0; n < ARRAY_SIZE(port_n_pins); n++)
    {
        rcc_periph_clock_enable(PORT_TO_RCC(port_n_pins[n].port));
        gpio_mode_setup(port_n_pins[n].port,
                        GPIO_MODE_ANALOG,
                        GPIO_PUPD_NONE,
                        port_n_pins[n].pins);
    }
    // Setup the adc(s)
    adcs_setup_adc();
}
 /// 10,ADC_CHANNEL_VREF,ADC_CHANNEL_TEMP,ADC_CHANNEL_VBAT
void adcs_cb(char* args)
{
    log_out("Pin 10 : %"PRIu16, adcs_read(ADC1, 10));
    log_out("VREF   : %"PRIu16, adcs_read(ADC1, ADC_CHANNEL_VREF));
    log_out("TEMP   : %"PRIu16, adcs_read(ADC1, ADC_CHANNEL_TEMP));
    log_out("VBAT   : %"PRIu16, adcs_read(ADC1, ADC_CHANNEL_VBAT));
}
