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


#define ADC_CCR_PRESCALE_1     0x0  /* 0b0000 */
#define ADC_CCR_PRESCALE_2     0x1  /* 0b0001 */
#define ADC_CCR_PRESCALE_4     0x2  /* 0b0010 */
#define ADC_CCR_PRESCALE_6     0x3  /* 0b0011 */
#define ADC_CCR_PRESCALE_8     0x4  /* 0b0100 */
#define ADC_CCR_PRESCALE_10    0x5  /* 0b0101 */
#define ADC_CCR_PRESCALE_12    0x6  /* 0b0110 */
#define ADC_CCR_PRESCALE_16    0x7  /* 0b0111 */
#define ADC_CCR_PRESCALE_32    0x8  /* 0b1000 */
#define ADC_CCR_PRESCALE_64    0x9  /* 0b1001 */
#define ADC_CCR_PRESCALE_128   0xA  /* 0b1010 */
#define ADC_CCR_PRESCALE_256   0xB  /* 0b1011 */
#define ADC_CCR_PRESCALE_MASK  0xF
#define ADC_CCR_PRESCALE_SHIFT 18

/* 640.5 + 12.5 cycles of (80Mhz / 64) clock
 * (640.5 + 12.5) * (1000000 / (80000000 / 64)) = 522.4 microseconds
 *
 * 522.4 * 480 = 250752 microseconds
 *
 * So 1/4 a second for all samples.
 *
 */


typedef uint16_t adcs_all_buf_t[ADCS_NUM_SAMPLES];

#define ADCS_MON_DEFAULT_COLLECTION_TIME    100;

static adcs_all_buf_t       _adcs_buffer;
static volatile bool        _adcs_in_use        = false;
static adcs_keys_t          _adcs_active_key    = ADCS_KEY_NONE;
static uint32_t             _adcs_start_time    = 0;
static volatile uint32_t    _adcs_end_time      = 0;


/* As the ADC RMS function calculates the RMS of potentially multiple ADCs in a single 
 * buffer, the step and start index are required to find the correct RMS.*/
#ifdef __ADC_RMS_FULL__
static bool _adcs_get_rms(const adcs_all_buf_t buff, unsigned buff_len, uint32_t* adc_rms, uint8_t start_index, uint8_t step, uint32_t midpoint)
{
    double inter_val = 0;
    int32_t mp_small = midpoint / 1000;
    for (unsigned i = start_index; i < buff_len; i+=step)
    {
        int64_t v = buff[i] - mp_small;
        inter_val += v * v;
    }
    adc_debug("inter_val = %.03lf", inter_val/1000.f);
    inter_val /= (buff_len / step);
    inter_val = sqrt(inter_val);
    inter_val *= 1000;
    inter_val = midpoint - inter_val;
    adc_debug("inter_val = %.03lf", inter_val/1000.f);
    *adc_rms = inter_val;
    adc_debug("RMS = %"PRIu32".%03"PRIu32, *adc_rms/1000, *adc_rms%1000);
    return true;
}
#else
static uint16_t                     peak_vals[ADCS_NUM_SAMPLES];
static bool _adcs_get_rms(const adcs_all_buf_t buff, unsigned buff_len, uint32_t* adc_rms, uint8_t start_index, uint8_t step, uint32_t midpoint)
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
    *adc_rms = midpoint - inter_val * 1000;
    adc_debug("RMS = %"PRIu32"%.03"PRIu32, *adc_rms/1000, *adc_rms%1000);
    return true;
}
#endif //__ADC_RMS_FULL__


static bool _adcs_get_avg(const adcs_all_buf_t buff, unsigned buff_len, uint32_t* adc_avg, uint8_t start_index, uint8_t step)
{
    uint64_t sum = 0;
    for (unsigned i = start_index; i < buff_len; i+=step)
    {
        sum += buff[i];
    }
    buff_len /= step;
    sum *= 1000;
    *adc_avg = sum / buff_len;
    adc_debug("AVG = %"PRIu32".%03"PRIu32, *adc_avg/1000, *adc_avg%1000);
    return true;
}


static void _adcs_set_prescale(uint32_t adc, uint32_t prescale)
{
    ADC_CCR(adc) &= ~(ADC_CCR_PRESCALE_MASK << ADC_CCR_PRESCALE_SHIFT);
    ADC_CCR(adc) |= (prescale & ADC_CCR_PRESCALE_MASK) << ADC_CCR_PRESCALE_SHIFT;
}


static void _adcs_setup_adc(void)
{
    adc_power_off(ADC1);
    RCC_CCIPR |= RCC_CCIPR_ADCSEL_SYS << RCC_CCIPR_ADCSEL_SHIFT;
    if (ADC_CR(ADC1) & ADC_CR_DEEPPWD)
    {
        ADC_CR(ADC1) &= ~ADC_CR_DEEPPWD;
    }

    _adcs_set_prescale(ADC1, ADC_CCR_PRESCALE_64);

    adc_enable_regulator(ADC1);
    adc_set_right_aligned(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_640DOT5CYC);
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


// cppcheck-suppress unusedFunction ; System handler
void dma1_channel1_isr(void)  /* ADC1 dma interrupt */
{
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF))
    {
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
        _adcs_in_use = false;
        _adcs_end_time = get_since_boot_ms();
    }
}


adcs_resp_t adcs_begin(uint8_t* channels, unsigned num_channels, unsigned num_samples, adcs_keys_t key)
{
    if (!channels || !num_channels)
        return ADCS_RESP_FAIL;

    if (_adcs_in_use)
        return ADCS_RESP_WAIT;

    if (_adcs_active_key != ADCS_KEY_NONE)
    {
        //adc_debug("Still locked.");
        return ADCS_RESP_WAIT;
    }

    if (num_samples > ADCS_NUM_SAMPLES)
    {
        adc_debug("ADC buffer too small for that many samples.");
        return ADCS_RESP_FAIL;
    }

    _adcs_in_use = true;
    _adcs_active_key = key;
    _adcs_start_time = get_since_boot_ms();

    dma_disable_channel(DMA1, DMA_CHANNEL1);
    dma_set_number_of_data(DMA1, DMA_CHANNEL1, num_samples);
    dma_enable_channel(DMA1, DMA_CHANNEL1);
    adc_set_regular_sequence(ADC1, num_channels, channels);
    adc_start_conversion_regular(ADC1);
    return ADCS_RESP_OK;
}


adcs_resp_t adcs_collect_rms(uint32_t* rms, uint32_t midpoint, unsigned num_channels, unsigned num_samples, unsigned cc_index, adcs_keys_t key, uint32_t* time_taken)
{
    if (!rms)
    {
        adc_debug("Handed NULL pointer.");
        return ADCS_RESP_FAIL;
    }
    if (_adcs_in_use)
    {
        return ADCS_RESP_WAIT;
    }
    if (_adcs_active_key != key)
        return ADCS_RESP_WAIT;
    if (!_adcs_get_rms(_adcs_buffer, num_samples, rms, cc_index, num_channels, midpoint))
    {
        adc_debug("Could not get RMS value for pos %u", cc_index);
        return ADCS_RESP_FAIL;
    }
    if (time_taken)
        *time_taken = since_boot_delta(_adcs_end_time, _adcs_start_time);
    return ADCS_RESP_OK;
}


adcs_resp_t adcs_collect_rmss(uint32_t* rmss, uint32_t* midpoints, unsigned num_channels, unsigned num_samples, adcs_keys_t key, uint32_t* time_taken)
{
    if (!rmss || !midpoints)
    {
        adc_debug("Handed NULL pointer.");
        return ADCS_RESP_FAIL;
    }
    if (_adcs_in_use)
    {
        return ADCS_RESP_WAIT;
    }
    if (_adcs_active_key != key)
        return ADCS_RESP_WAIT;
    for (unsigned i = 0; i < num_channels; i++)
    {
        if (!rmss || !midpoints)
        {
            adc_debug("Handed NULL pointer.");
            return ADCS_RESP_FAIL;
        }
        if (!_adcs_get_rms(_adcs_buffer, num_samples, &rmss[i], i, num_channels, midpoints[i]))
        {
            adc_debug("Could not get RMS value for pos %u", i);
            return ADCS_RESP_FAIL;
        }
    }
    if (time_taken)
        *time_taken = since_boot_delta(_adcs_end_time, _adcs_start_time);
    return ADCS_RESP_OK;
}


adcs_resp_t adcs_collect_avgs(uint32_t* avgs, unsigned num_channels, unsigned num_samples, adcs_keys_t key, uint32_t* time_taken)
{
    if (!avgs)
    {
        adc_debug("Handed NULL pointer.");
        return ADCS_RESP_FAIL;
    }
    if (_adcs_in_use)
    {
        return ADCS_RESP_WAIT;
    }
    if (_adcs_active_key != key)
        return ADCS_RESP_WAIT;
    for (unsigned i = 0; i < num_channels; i++)
    {
        if (!avgs)
        {
            adc_debug("Handed NULL pointer.");
            return ADCS_RESP_FAIL;
        }
        if (!_adcs_get_avg(_adcs_buffer, num_samples, avgs++, i, num_channels))
        {
            adc_debug("Could not get RMS value for pos %u", i);
            return ADCS_RESP_FAIL;
        }
    }
    if (time_taken)
        *time_taken = since_boot_delta(_adcs_end_time, _adcs_start_time);
    return ADCS_RESP_OK;
}


adcs_resp_t adcs_wait_done(uint32_t timeout, adcs_keys_t key)
{
    uint32_t start = get_since_boot_ms();
    if (_adcs_active_key != key)
        return ADCS_RESP_FAIL;
    while (_adcs_in_use)
    {
        if (since_boot_delta(get_since_boot_ms(), start) > timeout)
            return ADCS_RESP_FAIL;
    }
    return ADCS_RESP_OK;
}


void adcs_release(adcs_keys_t key)
{
    if (_adcs_active_key != key)
        return;
    _adcs_active_key = ADCS_KEY_NONE;
}


void adcs_off(void)
{
    adc_power_off(ADC1);
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
