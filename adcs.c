#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "sys_time.h"


#define ADC_CCR_PRESCALE_MASK  (15 << 18)
#define ADC_CCR_PRESCALE_64    ( 9 << 18)


#define ADCS_CONFIG_PRESCALE        ADC_CCR_PRESCALE_64
#define ADCS_CONFIG_SAMPLE_TIME     ADC_SMPR_SMP_640DOT5CYC


#define ADCS_DEFAULT_COLLECTION_TIME    5000;


#define ADCS_NUM_SAMPLES            480
#define ADCS_TIMEOUT_TIME_MS        5000


typedef enum
{
    ADCS_VAL_STATUS_IDLE,
    ADCS_VAL_STATUS_DOING,
    ADCS_VAL_STATUS_DONE,
} adc_value_status_t;


typedef enum
{
    ADCS_ADC_INDEX_ADC1    = 0 ,
    ADCS_ADC_INDEX_ADC2    = 1 ,
} adcs_adc_index_enum_t;


typedef enum
{
    ADCS_CHAN_INDEX_CURRENT_CLAMP  = 0 ,
    ADCS_CHAN_INDEX_BAT_MON        = 1 ,
    ADCS_CHAN_INDEX_3V3_MON        = 2 ,
    ADCS_CHAN_INDEX_5V_MON         = 3 ,
} adcs_chan_index_enum_t;


static adc_dma_channel_t            adc_dma_channels[]                                  = ADC_DMA_CHANNELS;
static uint16_t                     adcs_buffer[ADC_DMA_CHANNELS_COUNT][ADCS_NUM_SAMPLES*ADC_COUNT];
static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     midpoint;
static volatile adc_value_status_t  adc_value_status                                    = ADCS_VAL_STATUS_IDLE;
static uint16_t                     peak_vals[ADCS_NUM_SAMPLES];


uint32_t adcs_collection_time(void)
{
    /**
    Could calculate how long it should take to get the results. For now use 5 seconds.
    */
    return ADCS_DEFAULT_COLLECTION_TIME;
}


static float _Q_rsqrt( float number )
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
    y  = * ( float * ) &i;
    #pragma GCC diagnostic pop
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//  y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}


static bool _adcs_set_prescale(uint32_t adc, uint32_t prescale)
{
    if (((ADC_CR(adc) & ADC_CR_ADCAL    )==0) &&
        ((ADC_CR(adc) & ADC_CR_JADSTART )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADSTART  )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADSTP    )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADDIS    )==0) &&
        ((ADC_CR(adc) & ADC_CR_ADEN     )==0) )
    {
        ADC_CCR(adc) = (ADC_CCR(adc) & ADC_CCR_PRESCALE_MASK) | prescale;
        return true;
    }
    return false;
}


static void _adcs_setup_adc(void)
{
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    adc_power_off(ADC1);
    if (ADC_CR(ADC1) & ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    if (!_adcs_set_prescale(ADC1, ADCS_CONFIG_PRESCALE))
    {
        log_debug(DEBUG_ADC, "Could not set prescale value.");
    }

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_enable_vrefint();
    adc_enable_temperature_sensor();
    adc_set_sample_time_on_all_channels(ADC1, ADCS_CONFIG_SAMPLE_TIME);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

    adc_calibrate(ADC1);

    adc_set_continuous_conversion_mode(ADC1);
}


static void _adcs_setup_dma(adc_dma_channel_t* adc_dma, unsigned index)
{
    rcc_periph_clock_enable(adc_dma->dma_rcc);

    nvic_enable_irq(adc_dma->dma_irqn);
    dma_set_channel_request(adc_dma->dma_unit, adc_dma->dma_channel, 0);

    dma_channel_reset(adc_dma->dma_unit, adc_dma->dma_channel);

    dma_set_peripheral_address(adc_dma->dma_unit, adc_dma->dma_channel, (uint32_t)&ADC_DR(adc_dma->adc_unit));
    dma_set_memory_address(adc_dma->dma_unit, adc_dma->dma_channel, (uint32_t)(adcs_buffer[index]));
    dma_enable_memory_increment_mode(adc_dma->dma_unit, adc_dma->dma_channel);
    dma_set_peripheral_size(adc_dma->dma_unit, adc_dma->dma_channel, DMA_CCR_PSIZE_16BIT);
    dma_set_memory_size(adc_dma->dma_unit, adc_dma->dma_channel, DMA_CCR_MSIZE_16BIT);
    dma_set_priority(adc_dma->dma_unit, adc_dma->dma_channel, adc_dma->priority);

    dma_enable_transfer_complete_interrupt(adc_dma->dma_unit, adc_dma->dma_channel);
    dma_set_number_of_data(adc_dma->dma_unit, adc_dma->dma_channel, ADCS_NUM_SAMPLES * ADC_COUNT);
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


static bool _adcs_get_mean(uint16_t buff[ADC_DMA_CHANNELS_COUNT][ADCS_NUM_SAMPLES*ADC_COUNT], unsigned adc_index, unsigned chan_index, uint16_t* adc_mean)
{
    uint32_t sum = 0;
    for (unsigned i = chan_index; i < ADCS_NUM_SAMPLES*ADC_COUNT; i += ADC_COUNT)
    {
        sum += buff[adc_index][i];
    }
    *adc_mean = sum / ADCS_NUM_SAMPLES;
    return true;
}


static bool _adcs_get_rms_full(uint16_t buff[ADC_DMA_CHANNELS_COUNT][ADCS_NUM_SAMPLES*ADC_COUNT], unsigned adc_index, unsigned chan_index, uint16_t* adc_rms)
{
    uint64_t sum = 0;
    for (unsigned i = chan_index; i < ADCS_NUM_SAMPLES*ADC_COUNT; i += ADC_COUNT)
    {
        sum += buff[adc_index][i] * buff[adc_index][i];
    }
    *adc_rms = midpoint - 1/_Q_rsqrt( ( sum / ADCS_NUM_SAMPLES ) - midpoint );
    return true;
}


static bool _adcs_get_rms_quick(uint16_t buff[ADC_DMA_CHANNELS_COUNT][ADCS_NUM_SAMPLES*ADC_COUNT], unsigned adc_index, unsigned chan_index, uint16_t* adc_rms)
{
    /**
    Four major steps:
    * First find the peaks, this is done with the following criteria:
        - adc val is smaller than previous
        - new wave (and so move position/cursor) when before was below midpoint, now is above midpoint

    * Calculate 'rough' average

    * If peak values are larger than 30% of average then remove.
      This will remove the small noises that may fit the criteria but not be true peaks.

    * This value is divided by sqrt(2) to give the RMS.
    */

    uint32_t sum = 0;
    uint16_t peak_pos = 0;
    peak_vals[0] = buff[adc_index][chan_index];

    // Find the peaks in the data
    for (unsigned i = chan_index + ADC_COUNT; i < ADCS_NUM_SAMPLES*ADC_COUNT; i += ADC_COUNT)
    {
        if (buff[adc_index][i] < buff[adc_index][i-ADC_COUNT])
        {
            peak_vals[peak_pos] = buff[adc_index][i];
        }
        else if (buff[adc_index][i-ADC_COUNT] <= midpoint && buff[adc_index][i] > midpoint)
        {
            sum += peak_vals[peak_pos];
            peak_vals[++peak_pos] = buff[adc_index][i];
        }
    }

    // Early exit if nothing found
    if (peak_pos == 0)
    {
        log_debug(DEBUG_ADC, "Cannot find any peaks.");
        return false;
    }
    uint16_t rough_avg = sum / peak_pos;

    /*
    // Filter peaks
    uint32_t true_sum = 0;
    uint16_t true_count = 0;
    for (unsigned i = 0; i < peak_pos; i++)
    {
        if (peak_vals[i] >= rough_avg * 1.1)
        {
            true_sum += peak_vals[i];
            true_count++;
        }
    }
    *adc_rms = true_sum / true_count;
    */

    uint32_t inter_val = midpoint - rough_avg;
    inter_val *= 0.707106781187;                // * 1/sqrt(2)
    *adc_rms = midpoint - inter_val;
    return true;
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


static bool _adcs_current_clamp_conv(uint16_t* adc_val, uint16_t* cc_mA)
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
    uint32_t inter_value    = 0;
    uint16_t adc_diff       = 0;

    // If adc_val is larger, then pretend it is at the midpoint
    if (*adc_val < midpoint)
    {
        adc_diff = midpoint - *adc_val;
    }

    // Once the conversion is no longer linearly multiplicative this needs to be changed.
    if (!_adcs_to_mV(&adc_diff, (uint16_t*)&inter_value))
    {
        log_debug(DEBUG_ADC, "Cannot get mV value of midpoint.");
        return false;
    }

    if (inter_value > UINT32_MAX / 2000)                        // Division should be removed/boiled away by compiler
    {
        log_debug(DEBUG_ADC, "Overflowing value.");
        return false;
    }
    inter_value *= 2000;
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


bool adcs_begin(char* name)
{
    if (adc_value_status != ADCS_VAL_STATUS_IDLE)
    {
        log_debug(DEBUG_ADC, "Cannot begin, not in idle state.");
        return false;
    }
    adc_power_on(ADC1);
    _adcs_start_sampling(ADC1, adc_channel_array, ADC_COUNT);
    adc_value_status = ADCS_VAL_STATUS_DOING;
    return true;
}


static bool _adcs_collect_index(uint16_t* value, unsigned adc_index, unsigned chan_index)
{
    if (!value)
    {
        log_debug(DEBUG_ADC, "Given null pointer.");
        return false;
    }
    if (adc_value_status != ADCS_VAL_STATUS_DONE)
    {
        //log_debug(DEBUG_ADC, "ADC not ready to collect.");
        return false;
    }
    if (adc_index > ADC_DMA_CHANNELS_COUNT)
    {
        log_debug(DEBUG_ADC, "ADC index out of range.");
        return false;
    }
    if (chan_index > ADC_COUNT)
    {
        log_debug(DEBUG_ADC, "Channel index out of range.");
        return false;
    }
    adc_value_status = ADCS_VAL_STATUS_IDLE;
    return _adcs_get_mean(adcs_buffer, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP, value);
}


bool adcs_collect(char* name, value_t* value)
{
    unsigned adc_index, chan_index;
    if (strncmp(name, "CuCl", 4) == 0)
    {
        adc_index = ADCS_ADC_INDEX_ADC1;
        chan_index = ADCS_CHAN_INDEX_CURRENT_CLAMP;
    }
    else if (strncmp(name, "batt", 4) == 0)
    {
        adc_index = ADCS_ADC_INDEX_ADC1;
        chan_index = ADCS_CHAN_INDEX_BAT_MON;
    }
    else if (strncmp(name, "3vrf", 4) == 0)
    {
        adc_index = ADCS_ADC_INDEX_ADC1;
        chan_index = ADCS_CHAN_INDEX_3V3_MON;
    }
    else if (strncmp(name, "5vrf", 4) == 0)
    {
        adc_index = ADCS_ADC_INDEX_ADC1;
        chan_index = ADCS_CHAN_INDEX_5V_MON;
    }
    else
    {
        log_debug(DEBUG_ADC, "Unknown name given.");
        return false;
    };
    *value = 0;
    return _adcs_collect_index((uint16_t*)value, adc_index, chan_index);
}


static bool _adcs_wait(uint16_t* value, unsigned adc_index, unsigned chan_index)
{
    // This function is blocking and only should be used for debug
    if (!value)
    {
        log_debug(DEBUG_ADC, "Given null pointer.");
        return false;
    }
    if (!adcs_begin("NONE"))
    {
        log_debug(DEBUG_ADC, "Begin sampling failed.");
        return false;
    }
    while(!_adcs_collect_index(value, adc_index, chan_index));
    return true;
}


bool adcs_calibrate_current_clamp(void)
{
    // This function is blocking and only should be used for debug/calibration
    uint16_t new_midpoint = 0;
    if (!_adcs_wait(&new_midpoint, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP))
    {
        log_debug(DEBUG_ADC, "Could not retreive adc value to set as midpoint.");
        return false;
    }
    return adcs_set_midpoint(new_midpoint);
}


bool adcs_get_cc_blocking(char* name, value_t* value)
{
    // This function is blocking and only should be used for debug
    if (!value)
    {
        log_debug(DEBUG_ADC, "Given null pointer.");
        return false;
    }
    if (!adcs_begin(""))
    {
        log_debug(DEBUG_ADC, "Could not retrieve adc value.");
        return false;
    }
    uint32_t start_time = since_boot_ms;
    while (adc_value_status != ADCS_VAL_STATUS_DONE)
    {
        if (since_boot_delta(since_boot_ms, start_time) > ADCS_TIMEOUT_TIME_MS)
        {
            log_debug(DEBUG_ADC, "ADC request timed out.");
            adc_power_off(ADC1);
            adc_value_status = ADCS_VAL_STATUS_IDLE;
            //_adcs_setup_dmas();
            return false;
        }
    }
    adc_value_status = ADCS_VAL_STATUS_IDLE;
    uint16_t adc_rms = 0;
    uint16_t mA_val = 0;
    _adcs_get_rms_full(adcs_buffer, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP, &adc_rms);
    log_debug(DEBUG_ADC, "Full = %"PRIu16, adc_rms);
    _adcs_get_rms_quick(adcs_buffer, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP, &adc_rms);
    log_debug(DEBUG_ADC, "Quick = %"PRIu16, adc_rms);
    if (!_adcs_current_clamp_conv(&adc_rms, &mA_val))
    {
        log_debug(DEBUG_ADC, "Could not convert adc value into mA.");
        return false;
    }
    *value = 0;
    *value = mA_val;
    return true;
}


bool adcs_get_cc(char* name, value_t* value)
{
    // This function is blocking and only should be used for debug
    if (!value)
    {
        log_debug(DEBUG_ADC, "Given null pointer.");
        return false;
    }
    if (adc_value_status != ADCS_VAL_STATUS_DONE)
    {
        log_debug(DEBUG_ADC, "No ADC value ready.");
        adc_power_off(ADC1);
        adc_value_status = ADCS_VAL_STATUS_IDLE;
        return false;
    }
    adc_value_status = ADCS_VAL_STATUS_IDLE;
    uint16_t adc_rms = 0;
    uint16_t mA_val = 0;
    _adcs_get_rms_full(adcs_buffer, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP, &adc_rms);
    log_debug(DEBUG_ADC, "Full = %"PRIu16, adc_rms);
    _adcs_get_rms_quick(adcs_buffer, ADCS_ADC_INDEX_ADC1, ADCS_CHAN_INDEX_CURRENT_CLAMP, &adc_rms);
    log_debug(DEBUG_ADC, "Quick = %"PRIu16, adc_rms);
    if (!_adcs_current_clamp_conv(&adc_rms, &mA_val))
    {
        log_debug(DEBUG_ADC, "Could not convert adc value into mA.");
        return false;
    }
    *value = 0;
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
        adcs_set_midpoint(2048); // = 4095 / 2
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
        adc_power_off_async(ADC1);
        adc_value_status = ADCS_VAL_STATUS_DONE;
        ADC_ISR(ADC1) &= ~ADC_ISR_EOSMP;
    }
}
