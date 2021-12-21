#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "sys_time.h"


#define NUM_SAMPLES     128


typedef enum
{
    ADCS_VAL_STATUS__IDLE,
    ADCS_VAL_STATUS__DOING,
    ADCS_VAL_STATUS__DONE,
} adc_value_status_t;


typedef struct
{
    uint32_t        sum;
    uint16_t        count;
    uint16_t        max;
    uint16_t        min;
} adc_reading_t;


static uint64_t                     call_count                                          = 0;
static adc_dma_channel_t            adc_dma_channels[]                                  = ADC_DMA_CHANNELS;
static uint16_t                     adcs_buffer[ADC_DMA_CHANNELS_COUNT][NUM_SAMPLES*ADC_COUNT];
static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     midpoint;
static volatile adc_value_status_t  adc_value_status                                    = ADCS_VAL_STATUS__IDLE;


static void _adcs_setup_adc(void)
{
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    adc_power_off(ADC1);
    if (ADC_CR(ADC1) && ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_enable_vrefint();
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_6DOT5CYC);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

    adc_calibrate(ADC1);
    //adc_power_on(ADC1);

    //adc_set_regular_sequence(ADC1, ADC_COUNT, adc_channel_array);
    adc_set_continuous_conversion_mode(ADC1);
}


static void _adcs_setup_dma(adc_dma_channel_t* adc_dma, unsigned index)
{
    rcc_periph_clock_enable(adc_dma->dma_rcc);

    nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);
    dma_set_channel_request(adc_dma->dma_unit, adc_dma->dma_channel, 0);

    dma_channel_reset(adc_dma->dma_unit, adc_dma->dma_channel);

    dma_set_peripheral_address(adc_dma->dma_unit, adc_dma->dma_channel, (uint32_t)&ADC_DR(adc_dma->adc_unit));
    dma_set_memory_address(adc_dma->dma_unit, adc_dma->dma_channel, (uint32_t)(adcs_buffer[index]));
    dma_enable_memory_increment_mode(adc_dma->dma_unit, adc_dma->dma_channel);
    dma_set_peripheral_size(adc_dma->dma_unit, adc_dma->dma_channel, DMA_CCR_PSIZE_16BIT);
    dma_set_memory_size(adc_dma->dma_unit, adc_dma->dma_channel, DMA_CCR_MSIZE_16BIT);
    dma_set_priority(adc_dma->dma_unit, adc_dma->dma_channel, adc_dma->priority);

    dma_enable_transfer_complete_interrupt(adc_dma->dma_unit, adc_dma->dma_channel);
    dma_set_number_of_data(adc_dma->dma_unit, adc_dma->dma_channel, NUM_SAMPLES * ADC_COUNT);
    dma_enable_circular_mode(adc_dma->dma_unit, adc_dma->dma_channel);
    dma_set_read_from_peripheral(adc_dma->dma_unit, adc_dma->dma_channel);

    if (adc_dma->enabled)
    {
        dma_enable_channel(adc_dma->dma_unit, adc_dma->dma_channel);
    }
    adc_enable_dma(adc_dma->adc_unit);
}

static void _adcs_setup_dmas(void)
{
    for (unsigned i = 0; i < ADC_DMA_CHANNELS_COUNT; i++)
    {
        _adcs_setup_dma(&adc_dma_channels[i], i);
    }
}


static void _adcs_data_compress(volatile uint16_t buff[NUM_SAMPLES], adc_reading_t* data, uint8_t len)
{
    volatile uint16_t* element;
    adc_reading_t* p = data;
    for (unsigned i = 0; i < len; i++)
    {
        p->sum = 0;
        p->count = 0;
        p->max = 0;
        p->min = UINT16_MAX;
        for (unsigned j = 0; j < NUM_SAMPLES; j++)
        {
            element = &buff[i + j * len];
            if (*element > p->max)
            {
                p->max = *element;
            }
            if (*element < p->min)
            {
                p->min = *element;
            }
            p->sum += *element;
            p->count++;
        }
        p++;
    }
}


static bool _adcs_to_mV(uint16_t* value, uint16_t* mV)
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


static bool _adcs_current_clamp_conv(bool is_AC, uint16_t* adc_mV, uint16_t* cc_mA)
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
    uint16_t mp_mV;
    if (!_adcs_to_mV((uint16_t*)&midpoint, &mp_mV))
    {
        log_debug(DEBUG_ADC, "Cannot get mV value of midpoint.");
        return false;
    }

    // If adc_mV is larger then pretend it is at the midpoint
    if (*adc_mV > mp_mV)
    {
        inter_value = 0;
    }
    else
    {
        inter_value = mp_mV - *adc_mV;
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


static void _adcs_start_sampling(uint32_t adc, uint8_t* channels, uint8_t len)
{
    call_count = 0;
    adc_set_regular_sequence(adc, len, channels);
    adc_start_conversion_regular(adc);
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


void adcs_loop_iteration(void)
{
    ;
}


void temp(char* args)
{
    uint16_t adc_mV = 1800;
    uint16_t cc_mA;
    _adcs_current_clamp_conv(false, &adc_mV, &cc_mA);
}


bool adcs_begin(char* name)
{
    if (adc_value_status == ADCS_VAL_STATUS__DONE)
    {
        return true;
    }
    else if (adc_value_status == ADCS_VAL_STATUS__DOING)
    {
        return false;
    }
    adc_enable_eoc_interrupt(ADC1);
    adc_power_on(ADC1);
    _adcs_start_sampling(ADC1, adc_channel_array, ADC_COUNT);
    return true;
}


bool adcs_collect(char* name, uint16_t* value)
{
    if (adc_value_status != ADCS_VAL_STATUS__DONE)
    {
        return false;
    }
    adc_value_status = ADCS_VAL_STATUS__IDLE;
    adc_reading_t cc_reading[ADC_COUNT];
    _adcs_data_compress(adcs_buffer[0], cc_reading, ADC_COUNT);
    if (cc_reading[0].count)
    {
        *value = cc_reading[0].sum / cc_reading[0].count;
    }
    else
    {
        *value = 0;
    }
    return true;
}


static bool _adcs_wait(uint16_t* value)
{
    if (!value)
    {
        return false;
    }
    if (!adcs_begin("NONE"))
    {
        return false;
    }
    while(!adcs_collect("NONE", value));
    return true;
}


bool adcs_calibrate_current_clamp(void)
{
    uint16_t new_midpoint;
    if (!_adcs_wait(&new_midpoint))
    {
        return false;
    }
    return adcs_set_midpoint(new_midpoint);
}


bool adcs_get_cc_mA(value_t* value)
{
    if (!value)
    {
        return false;
    }
    uint16_t adc_val, mV_val, mA_val;
    if (!_adcs_wait(&adc_val))
    {
        return false;
    }
    if (!_adcs_to_mV(&adc_val, &mV_val))
    {
        return false;
    }
    if (!_adcs_current_clamp_conv(false, &mV_val, &mA_val))
    {
        return false;
    }
    *value = mA_val;
    return true;
}


void adcs_init(void)
{
    // Get the midpoint
    if (!persist_get_adc_midpoint(&midpoint))
    {
        // Assume it to be the theoretical midpoint
        log_debug(DEBUG_ADC, "Failed to load persistent midpoint.");
        adcs_set_midpoint(3300 / 2);
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
    // Setup the dma(s)
    _adcs_setup_dmas();
    // Setup the adc(s)
    _adcs_setup_adc();
}


void dma1_channel1_isr(void)  /* ADC1 dma interrupt */
{
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF))
    {
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
        adc_power_off(ADC1);
        adc_value_status = ADCS_VAL_STATUS__DONE;
    }
    _adcs_setup_dmas();
}
