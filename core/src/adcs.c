#include <inttypes.h>
#include <math.h>
#include <stddef.h>

#include "adcs.h"

#include "common.h"
#include "config.h"
#include "log.h"
#include "platform.h"

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

static adc_setup_config_t _adcs_config = {.mem_addr = (uintptr_t)_adcs_buffer};


bool adcs_to_mV(uint32_t value, uint32_t* mV)
{
    // Linear scale without calibration.
    // ADC of    0 -> 0V
    //        4096 -> 3.3V

    const uint16_t max_value = ADC_MAX_VAL;
    const uint16_t min_value = 0;
    const uint16_t max_mV = ADC_MAX_MV;
    const uint16_t min_mV = 0;
    uint32_t inter_val;

    inter_val = value * (max_mV - min_mV);
    inter_val /= (max_value - min_value);

    *mV = inter_val;
    return true;
}


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


void adcs_dma_complete(void)
{
    _adcs_in_use = false;
    _adcs_end_time = get_since_boot_ms();
}


adcs_resp_t adcs_begin(adcs_type_t* channels, unsigned num_channels, unsigned num_samples, adcs_keys_t key)
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

    platform_adc_set_num_data(num_samples);
    platform_adc_set_regular_sequence(num_channels, channels);
    platform_adc_start_conversion_regular();
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


static bool _adcs_wait_loop_iteration(void* userdata)
{
    return !_adcs_in_use;
}


adcs_resp_t adcs_wait_done(uint32_t timeout, adcs_keys_t key)
{
    if (_adcs_active_key != key)
        return ADCS_RESP_FAIL;
    return main_loop_iterate_for(timeout, _adcs_wait_loop_iteration, NULL) ? ADCS_RESP_OK : ADCS_RESP_FAIL;
}


void adcs_release(adcs_keys_t key)
{
    if (_adcs_active_key != key)
        return;
    _adcs_active_key = ADCS_KEY_NONE;
}


void adcs_off(void)
{
    platform_adc_power_off();
}


void adcs_init(void)
{
    platform_setup_adc(&_adcs_config);
}
