#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include "cc.h"

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "pinmap.h"
#include "persist_config.h"
#include "uart_rings.h"


#define CC_DEFAULT_COLLECTION_TIME          1000
#define CC_TIMEOUT_MS                       2000
#define CC_NUM_SAMPLES                      ADCS_NUM_SAMPLES

#define CC_RESISTOR_OHM                     22


typedef struct
{
    uint8_t     channels[ADC_CC_COUNT];
    unsigned    len;
} cc_channels_active_t;


static uint8_t              _cc_adc_channel_array[ADC_CC_COUNT] = ADC_CC_CHANNELS;
static uint32_t             _cc_midpoints[ADC_CC_COUNT];
static cc_channels_active_t _cc_adc_channels_active             = {0};
static bool                 _cc_running[ADC_CC_COUNT]           = {false};
static uint32_t             _cc_collection_time                 = CC_DEFAULT_COLLECTION_TIME;
static cc_config_t*         _configs;


static bool _cc_to_mV(uint32_t value, uint32_t* mV)
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


static bool _cc_conv(uint32_t adc_val, uint32_t* cc_mA, uint32_t midpoint, uint32_t scale_factor)
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
    uint32_t adc_diff       = 0;

    // If adc_val is larger, then pretend it is at the midpoint
    if (adc_val < midpoint)
    {
        adc_diff = midpoint - adc_val;
    }

    adc_debug("Difference = %"PRIu32".%03"PRIu32, adc_diff/1000, adc_diff%1000);

    // Once the conversion is no longer linearly multiplicative this needs to be changed.
    if (!_cc_to_mV(adc_diff, &inter_value))
    {
        adc_debug("Cannot get mV value of midpoint.");
        return false;
    }

    if (inter_value / 1000 > UINT32_MAX / scale_factor)
    {
        adc_debug("Overflowing value.");
        return false;
    }
    inter_value *= scale_factor;
    inter_value /= (CC_RESISTOR_OHM * 1000);
    *cc_mA = inter_value;
    return true;
}


static bool _cc_find_active_channel_index(uint8_t* active_channel_index, uint8_t index)
{
    uint8_t adc_channel = _cc_adc_channel_array[index];
    for (unsigned i = 0; i < _cc_adc_channels_active.len; i++)
    {
        if (_cc_adc_channels_active.channels[i] == adc_channel)
        {
            *active_channel_index = i;
            return true;
        }
    }
    return false;
}


static bool _cc_get_index(uint8_t* index, char* name)
{
    if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 0;
    }
    else if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 1;
    }
    else if (strncmp(name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_LEN) == 0)
    {
        *index = 2;
    }
    else
    {
        adc_debug("'%s' is not a current clamp name.", name);
        return false;
    }
    return true;
}


static bool _cc_get_channel(uint8_t* channel, uint8_t index)
{
    if (!channel)
    {
        adc_debug("Handed NULL pointer.");
        return false;
    }
    switch (index)
    {
        case 0:
            *channel = ADC1_CHANNEL_CURRENT_CLAMP_1;
            return true;
        case 1:
            *channel = ADC1_CHANNEL_CURRENT_CLAMP_2;
            return true;
        case 2:
            *channel = ADC1_CHANNEL_CURRENT_CLAMP_3;
            return true;
        default:
            adc_debug("No channel for %"PRIu8, index);
            return false;
    }
}


static bool _cc_get_info(char* name, uint8_t* index, uint8_t* active_index, uint8_t* channel)
{
    uint8_t index_local;
    if (!_cc_get_index(&index_local, name))
    {
        adc_debug("Cannot get index.");
        return false;
    }
    if (active_index &&
        !_cc_find_active_channel_index(active_index, index_local))
    {
        adc_debug("Not in active channel.");
        return false;
    }
    if (channel &&
        !_cc_get_channel(channel, index_local))
    {
        adc_debug("Cannot get channel.");
        return false;
    }
    if (index)
        *index = index_local;
    return true;
}


static bool _cc_wait(void)
{
    adc_debug("Waiting for ADC CC");
    adcs_resp_t resp = adcs_wait_done(CC_TIMEOUT_MS, ADCS_KEY_CC);
    switch (resp)
    {
        case ADCS_RESP_FAIL:
            break;
        case ADCS_RESP_WAIT:
            break;
        case ADCS_RESP_OK:
            return true;
    }
    adc_debug("Timed out waiting for CC ADC.");
    return false;
}


bool cc_set_channels_active(uint8_t* active_channels, unsigned len)
{
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        if (_cc_running[i])
        {
            adc_debug("Cannot change phase, ADC reading in progress.");
            return false;
        }
    }
    if (len > ADC_CC_COUNT)
    {
        adc_debug("Not possible length of array.");
        return false;
    }
    memcpy(_cc_adc_channels_active.channels, active_channels, len * sizeof(uint8_t));
    _cc_adc_channels_active.len = len;
    adc_debug("Setting %"PRIu8" active channels.", len);
    return true;
}


static void _cc_release_auto(void)
{
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        if (_cc_running[i])
            return;
    }
    adcs_release(ADCS_KEY_CC);
}


static void _cc_release_all(void)
{
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        _cc_running[i] = false;
    }
    adcs_release(ADCS_KEY_CC);
}


measurements_sensor_state_t cc_collection_time(char* name, uint32_t* collection_time)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = _cc_collection_time * 1.1;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t cc_begin(char* name)
{
    if (!_cc_adc_channels_active.len)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint8_t index, active_index;

    if (!_cc_get_info(name, &index, &active_index, NULL))
    {
        adc_debug("Cannot get CC info.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        if (i == index)
            continue;
        if (_cc_running[i])
        {
            if (index >= ADC_CC_COUNT)
                return MEASUREMENTS_SENSOR_STATE_ERROR;
            _cc_running[index] = true;
            return MEASUREMENTS_SENSOR_STATE_SUCCESS;
        }
    }
    adcs_resp_t resp = adcs_begin(_cc_adc_channels_active.channels, _cc_adc_channels_active.len, CC_NUM_SAMPLES, ADCS_KEY_CC);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Failed to begin CC ADC.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            break;
    }

    if (index >= ADC_CC_COUNT)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    _cc_running[index] = true;
    adc_debug("Started ADC reading for CC.");
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


measurements_sensor_state_t cc_get(char* name, value_t* value)
{
    uint8_t index, active_index;
    if (!_cc_get_info(name, &index, &active_index, NULL))
    {
        adc_debug("Cannot get info.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (index >= ADC_CC_COUNT)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    if (!_cc_running[index])
    {
        adc_debug("ADCs were not running.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint32_t adcs_rms;
    uint32_t midpoint = _cc_midpoints[index];

    adcs_resp_t resp = adcs_collect_rms(&adcs_rms, midpoint, _cc_adc_channels_active.len, CC_NUM_SAMPLES, active_index, ADCS_KEY_CC, &_cc_collection_time);
    switch (resp)
    {
        case ADCS_RESP_FAIL:
            _cc_running[index] = false;
            _cc_release_auto();
            adc_debug("Failed to get RMS");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            _cc_running[index] = false;
            _cc_release_auto();
            break;
    }

    uint32_t cc_mA;

    uint32_t scale_factor = _configs[index].ext_max_mA / _configs[index].int_max_mA;
    if (!_cc_conv(adcs_rms, &cc_mA, midpoint, scale_factor))
    {
        adc_debug("Failed to get current clamp");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    *value = value_from(cc_mA);
    adc_debug("CC = %"PRIu32"mA", cc_mA);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool cc_set_midpoints(uint32_t new_midpoints[ADC_CC_COUNT])
{
    memcpy(_cc_midpoints, new_midpoints, ADC_CC_COUNT * sizeof(_cc_midpoints[0]));
    if (!persist_set_cc_midpoints(new_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


bool cc_set_midpoint(uint32_t midpoint, char* name)
{
    uint8_t index;
    if (!_cc_get_index(&index, name))
        return false;
    if (index >= ADC_CC_COUNT)
    {
        return false;
    }
    _cc_midpoints[index] = midpoint;
    if (!persist_set_cc_midpoints(_cc_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


bool cc_get_midpoint(uint32_t* midpoint, char* name)
{
    uint8_t index;
    if (!_cc_get_index(&index, name))
        return false;
    if (index >= ADC_CC_COUNT)
        return false;
    *midpoint = _cc_midpoints[index];
    return true;
}


bool cc_calibrate(void)
{
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    cc_channels_active_t prev_cc_adc_channels_active = {0};
    memcpy(prev_cc_adc_channels_active.channels, _cc_adc_channels_active.channels, _cc_adc_channels_active.len * sizeof(_cc_adc_channels_active.channels[0]));
    prev_cc_adc_channels_active.len = _cc_adc_channels_active.len;

    memcpy(_cc_adc_channels_active.channels, all_cc_channels, ADC_CC_COUNT);
    _cc_adc_channels_active.len = ADC_CC_COUNT;

    adcs_resp_t resp = adcs_begin(_cc_adc_channels_active.channels, _cc_adc_channels_active.len, CC_NUM_SAMPLES, ADCS_KEY_CC);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Failed to begin CC ADC.");
            return false;
        case ADCS_RESP_WAIT:
            return false;
        case ADCS_RESP_OK:
            break;
    }
    if (!_cc_wait())
    {
        _cc_release_all();
        return false;
    }
    uint32_t t_mp[ADC_CC_COUNT];
    resp = adcs_collect_avgs(t_mp, ADC_CC_COUNT, CC_NUM_SAMPLES, ADCS_KEY_CC, NULL);
    //_cc_release_all();
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Could not average the ADC.");
            return false;
        case ADCS_RESP_WAIT:
            adc_debug("Could not average the ADC.");
            return false;
        case ADCS_RESP_OK:
            break;
    }

    uint32_t midpoints[ADC_CC_COUNT];
    resp = adcs_collect_rmss(midpoints, t_mp, ADC_CC_COUNT, CC_NUM_SAMPLES, ADCS_KEY_CC, NULL);
    _cc_release_all();
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Could not average the ADC.");
            return false;
        case ADCS_RESP_WAIT:
            adc_debug("Could not average the ADC.");
            return false;
        case ADCS_RESP_OK:
            break;
    }

    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
        adc_debug("MP CC%u: %"PRIu32".%03"PRIu32, i+1, midpoints[i]/1000, midpoints[i]%1000);
    cc_set_midpoints(midpoints);
    memcpy(_cc_adc_channels_active.channels, prev_cc_adc_channels_active.channels, prev_cc_adc_channels_active.len);
    _cc_adc_channels_active.len = prev_cc_adc_channels_active.len;
    return true;
}


bool cc_get_blocking(char* name, value_t* value)
{
    uint8_t channel;
    if (!_cc_get_info(name, NULL, NULL, &channel))
    {
        adc_debug("Cannot get info.");
        return false;
    }
    cc_channels_active_t prev_active_channels;
    memcpy(prev_active_channels.channels, _cc_adc_channels_active.channels, _cc_adc_channels_active.len);
    prev_active_channels.len = _cc_adc_channels_active.len;
    if (!cc_set_channels_active(&channel, 1))
    {
        adc_debug("Cannot set active channel.");
        return false;
    }
    if (cc_begin(name) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }
    if (!_cc_wait())
        return false;
    bool r = (cc_get(name, value) == MEASUREMENTS_SENSOR_STATE_SUCCESS);
    _cc_release_all();
    cc_set_channels_active(prev_active_channels.channels, prev_active_channels.len);
    return r;
}


bool cc_get_all_blocking(value_t* value_1, value_t* value_2, value_t* value_3)
{
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    cc_channels_active_t prev_cc_adc_channels_active = {0};
    memcpy(prev_cc_adc_channels_active.channels, _cc_adc_channels_active.channels, _cc_adc_channels_active.len * sizeof(_cc_adc_channels_active.channels[0]));
    prev_cc_adc_channels_active.len = _cc_adc_channels_active.len;

    if (!cc_set_channels_active(all_cc_channels, ADC_CC_COUNT))
    {
        adc_debug("Cannot set active channel.");
        return false;
    }

    if (cc_begin(MEASUREMENTS_CURRENT_CLAMP_1_NAME) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (cc_begin(MEASUREMENTS_CURRENT_CLAMP_2_NAME) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (cc_begin(MEASUREMENTS_CURRENT_CLAMP_3_NAME) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (!_cc_wait())
        return false;
    char name[MEASURE_NAME_NULLED_LEN] = {0};
    if (cc_get(MEASUREMENTS_CURRENT_CLAMP_1_NAME, value_1) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    if (cc_get(MEASUREMENTS_CURRENT_CLAMP_2_NAME, value_2) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    if (cc_get(MEASUREMENTS_CURRENT_CLAMP_3_NAME, value_3) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    _cc_release_all();

    return cc_set_channels_active(prev_cc_adc_channels_active.channels, prev_cc_adc_channels_active.len);
bad_exit:
    _cc_release_all();
    adc_debug("Couldnt get %s value.", name);
    return false;
}


void cc_init(void)
{
    // Get the midpoints
    if (!persist_get_cc_midpoints(_cc_midpoints))
    {
        // Assume it to be the theoretical midpoint
        adc_debug("Failed to load persistent midpoint.");
        uint32_t midpoints[ADC_CC_COUNT] = {ADC_MAX_VAL / 2};
        cc_set_midpoints(midpoints);
    }
    _configs = persist_get_cc_configs();
}
