#include <inttypes.h>
#include <math.h>

#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "adcs.h"

#include "common.h"
#include "config.h"
#include "log.h"
#include "pinmap.h"


#define ADC_CCR_PRESCALE_MASK  (15 << 18)
#define ADC_CCR_PRESCALE_64    ( 9 << 18)


#define ADCS_CONFIG_PRESCALE        ADC_CCR_PRESCALE_64
#define ADCS_CONFIG_SAMPLE_TIME     ADC_SMPR_SMP_640DOT5CYC /* 640.5 + 12.5 cycles of (80Mhz / 64) clock */
/* (640.5 + 12.5) * (1000000 / (80000000 / 64)) = 522.4 microseconds
 *
 * 480 samples
 *
 * 522.4 * 480 = 250752 microseconds
 *
 * So 1/4 a second for all samples.
 *
 */


typedef uint16_t adcs_all_buf_t[ADCS_NUM_SAMPLES];


static adcs_all_buf_t           _adcs_buffer;
static volatile bool            _adcs_in_use             = false;


/* As the ADC RMS function calculates the RMS of potentially multiple ADCs in a single 
 * buffer, the step and start index are required to find the correct RMS.*/
#ifdef __ADC_RMS_FULL__
static bool _adcs_get_rms(adcs_all_buf_t buff, unsigned buff_len, uint16_t* adc_rms, uint8_t start_index, uint8_t step, uint16_t midpoint)
{
    uint64_t sum = 0;
    int64_t inter_val;
    for (unsigned i = start_index; i < buff_len; i+=step)
    {
        inter_val = buff[i] - midpoint;
        inter_val *= inter_val;
        sum += inter_val;
    }
    *adc_rms = midpoint - 1/Q_rsqrt( sum / (ADCS_NUM_SAMPLES/step) );
    adc_debug("RMS = %"PRIu16, *adc_rms);
    return true;
}
#else
static uint16_t                     peak_vals[ADCS_NUM_SAMPLES];
static bool _adcs_get_rms(adcs_all_buf_t buff, unsigned buff_len, uint16_t* adc_rms, uint8_t start_index, uint8_t step, uint16_t midpoint)
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
    peak_vals[0] = buff[start_index];

    // Find the peaks in the data
    for (unsigned i = (start_index + step); i < buff_len; i+=step)
    {
        if (buff[i] < buff[i-1])
        {
            peak_vals[peak_pos] = buff[i];
        }
        else if (buff[i-1] <= midpoint && buff[i] > midpoint)
        {
            sum += peak_vals[peak_pos];
            peak_vals[++peak_pos] = buff[i];
        }
    }

    // Early exit if nothing found
    if (peak_pos == 0)
    {
        adc_debug("Cannot find any peaks.");
        return false;
    }
    uint32_t inter_val = midpoint - sum / peak_pos;
    inter_val /= sqrt(2);
    *adc_rms = midpoint - inter_val;
    adc_debug("RMS = %"PRIu16, *adc_rms);
    return true;
}
#endif //__ADC_RMS_FULL__


static bool _adcs_get_avg(adcs_all_buf_t buff, unsigned buff_len, uint16_t* adc_avg, uint8_t start_index, uint8_t step)
{
    uint64_t sum = 0;
    for (unsigned i = start_index; i < buff_len; i+=step)
    {
        sum += buff[i];
    }
    *adc_avg = sum * step / ADCS_NUM_SAMPLES;
    adc_debug("AVG = %"PRIu16, *adc_avg);
    return true;
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
    adc_power_off(ADC1);
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    if (ADC_CR(ADC1) & ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    if (!_adcs_set_prescale(ADC1, ADCS_CONFIG_PRESCALE))
    {
        adc_debug("Could not set prescale value.");
    }

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADCS_CONFIG_SAMPLE_TIME);
    adc_set_resolution(ADC1, ADC_CFGR1_RES_12_BIT);

    adc_calibrate(ADC1);
    adc_set_continuous_conversion_mode(ADC1);
    adc_enable_dma(ADC1);
    adc_power_on(ADC1);
}


static void _adcs_setup_dma(void)
{
    rcc_periph_clock_enable(RCC_DMA1);

    nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);
    dma_set_channel_request(DMA1, DMA_CHANNEL1, 0);

    dma_channel_reset(DMA1, DMA_CHANNEL1);

    dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)&ADC_DR(ADC1));
    dma_set_memory_address(DMA1, DMA_CHANNEL1, (uint32_t)(_adcs_buffer));
    dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
    dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
    dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
    dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL1);
    dma_set_number_of_data(DMA1, DMA_CHANNEL1, ADCS_NUM_SAMPLES);
    dma_enable_circular_mode(DMA1, DMA_CHANNEL1);
    dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);

    dma_enable_channel(DMA1, DMA_CHANNEL1);
}


void dma1_channel1_isr(void)  /* ADC1 dma interrupt */
{
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF))
    {
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
        _adcs_in_use = false;
    }
}


bool adcs_begin(uint8_t* channels, unsigned size)
{
    if (!channels || !size)
        return false;

    if (_adcs_in_use)
        return false;
    _adcs_in_use = true;
    adc_set_regular_sequence(ADC1, size, channels);
    adc_start_conversion_regular(ADC1);
    return true;
}


bool adcs_collect_rms(uint16_t* rms, uint16_t midpoint, unsigned size, unsigned index)
{
    if (!rms)
    {
        adc_debug("Handed NULL pointer.");
        return false;
    }
    if (_adcs_in_use)
    {
        adc_debug("ADC in use");
        return false;
    }
    if (!_adcs_get_rms(_adcs_buffer, ADCS_NUM_SAMPLES, rms, index, size, midpoint))
    {
        adc_debug("Could not get RMS value for pos %u", index);
        return false;
    }
    return true;
}


bool adcs_collect_rmss(uint16_t* rmss, uint16_t* midpoints, unsigned size)
{
    if (!rmss || !midpoints)
    {
        adc_debug("Handed NULL pointer.");
        return false;
    }
    if (_adcs_in_use)
    {
        adc_debug("ADC in use");
        return false;
    }
    for (unsigned i = 0; i < size; i++)
    {
        if (!rmss || !midpoints)
        {
            adc_debug("Handed NULL pointer.");
            return false;
        }
        if (!_adcs_get_rms(_adcs_buffer, ADCS_NUM_SAMPLES, rmss, i, size, midpoints[i]))
        {
            adc_debug("Could not get RMS value for pos %u", i);
            return false;
        }
    }
    return true;
}


bool adcs_collect_avgs(uint16_t* avgs, unsigned size)
{
    if (!avgs)
    {
        adc_debug("Handed NULL pointer.");
        return false;
    }
    if (_adcs_in_use)
    {
        adc_debug("ADC in use");
        return false;
    }
    for (unsigned i = 0; i < size; i++)
    {
        if (!avgs)
        {
            adc_debug("Handed NULL pointer.");
            return false;
        }
        if (!_adcs_get_avg(_adcs_buffer, ADCS_NUM_SAMPLES, avgs++, i, size))
        {
            adc_debug("Could not get RMS value for pos %u", i);
            return false;
        }
    }
    return true;
}


bool adcs_wait_done(uint32_t timeout)
{
    uint32_t start = get_since_boot_ms();
    while (_adcs_in_use)
    {
        if (since_boot_delta(get_since_boot_ms(), start) > timeout)
            return false;
    }
    return true;
}


void adcs_init(void)
{
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
    _adcs_setup_dma();
    // Setup the adc(s)
    _adcs_setup_adc();
}
