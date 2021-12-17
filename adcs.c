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
uint16_t midpoint;


static void adcs_setup_adc(void)
{
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    adc_power_off(ADC1);
    if (ADC_CR(ADC1) && ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    adc_calibrate(ADC1);
    adc_set_single_conversion_mode(ADC1);
    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_enable_vrefint();
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_6DOT5CYC);
    adc_set_regular_sequence(ADC1, 1, adc_channel_array);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

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


static bool adcs_to_mV(uint16_t* value, uint16_t* mV)
{
    // Linear scale without calibration.
    // ADC of    0 -> 0V
    //        4096 -> 3.3V

    uint16_t max_value = 4095;
    uint16_t min_value = 0;
    uint16_t max_mV = 3300;
    uint16_t min_mV = 0;
    uint32_t inter_val;

    inter_val = *value * (max_mV - min_mV);
    inter_val /= (max_value - min_value);

    if (inter_val > UINT16_MAX)
    {
        log_debug(DEBUG_ADC, "Cannot downsize value '%"PRIu32"'.", inter_val);
        return false;
    }

    *mV = (uint16_t)inter_val;
    return true;
}


static bool adcs_current_clamp_conv(bool is_AC, uint16_t* adc_mV, uint16_t* cc_mA)
{
    /**
     First must calculate the peak voltage
     V_pk = Midpoint - V_adc

     Next convert peak voltage to RMS. For DC this is "=" to it. Kind of.
     For AC it can approximated by V_RMS = 1/sqrt(2) * V_peak

     Now converting from a voltage to a current we use our trusty
     V = I * R      - >     I = V / R
     The resistor used is 22 Ohms.

     The current is scaled by the current clamp to the effect of
     I_t = 2000 * I.

     To retain precision, multiplications should be done first (after casting to
     a uint32_t and then divisions after.
    */
    uint32_t inter_value;

    // If adc_mV is larger then pretend it is at the midpoint
    if (*adc_mV > midpoint)
    {
        inter_value = 0;
    }
    else
    {
        inter_value = midpoint - *adc_mV;
    }

    if (inter_value > UINT32_MAX / 2000)
    {
        log_debug(DEBUG_ADC, "Overflowing value.");
        return false;
    }
    inter_value *= 2000;
    if (is_AC)
    {
        inter_value /= 1.4142136;
    }
    inter_value /= 22;
    if (inter_value > UINT16_MAX)
    {
        log_debug(DEBUG_ADC, "Cannot downsize value '%"PRIu32"'.", inter_value);
        return false;
    }
    *cc_mA = (uint16_t)inter_value;
    return true;
}


bool adcs_get_current_clamp(uint16_t* cc_mA)
{
    uint16_t cc, cc_mV;
    cc = adcs_read(ADC1, 6);
    if (!adcs_to_mV(&cc, &cc_mV))
    {
        log_debug(DEBUG_ADC, "Failed to convert ADC value to mV");
        return false;
    }
    if (!adcs_current_clamp_conv(false, &cc_mV, cc_mA))
    {
        log_debug(DEBUG_ADC, "Failed to convert mV to mA");
        return false;
    }
    return true;
}


void adcs_cb(char* args)
{
    uint16_t cc_mA;
    adcs_get_current_clamp(&cc_mA);
    log_out("Current Clamp      : %"PRIu16"mA", cc_mA);
    log_out("BAT_MON            : %"PRIu16, adcs_read(ADC1, 1));
    log_out("3V3_RAIL_MONITOR   : %"PRIu16, adcs_read(ADC1, 3));
    log_out("5V_RAIL_MONITOR    : %"PRIu16, adcs_read(ADC1, 4));
}


bool adcs_set_midpoint(uint16_t new_midpoint)
{
    midpoint = new_midpoint;
    if (!persist_set_adc_midpoint(new_midpoint))
    {
        log_debug(DEBUG_ADC, "Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


void adcs_init(void)
{
    // Get the midpoint
    if (!persist_get_adc_midpoint(&midpoint))
    {
        // Assume it to be the theoretical midpoint
        midpoint = 3300 / 2;
    }

    // Setup the clock and gpios
    const port_n_pins_t port_n_pins[] = ADCS_PORT_N_PINS;
    rcc_periph_clock_enable(RCC_ADC1);
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
