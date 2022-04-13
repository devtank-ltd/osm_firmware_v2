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


#define ADCS_CC_DEFAULT_COLLECTION_TIME     1000;
#define CC_TIMEOUT_MS                       1000


typedef struct
{
    uint8_t     channels[ADC_CC_COUNT];
    unsigned    len;
} adcs_channels_active_t;


static uint8_t                      adc_channel_array[ADC_COUNT]                        = ADC_CHANNELS;
static uint16_t                     cc_midpoints[ADC_CC_COUNT];

static adcs_channels_active_t       adc_cc_channels_active                              = {0};

static volatile bool                adcs_cc_running                                     = false;


bool adcs_cc_set_channels_active(uint8_t* active_channels, unsigned len)
{
    if (adcs_cc_running)
    {
        adc_debug("Cannot change phase, ADC reading in progress.");
        return false;
    }
    if (len > ADC_CC_COUNT)
    {
        adc_debug("Not possible length of array.");
        return false;
    }
    memcpy(adc_cc_channels_active.channels, active_channels, len * sizeof(uint8_t));
    adc_cc_channels_active.len = len;
    adc_debug("Setting %"PRIu8" active channels.", len);
    return true;
}


measurements_sensor_state_t adcs_cc_collection_time(char* name, uint32_t* collection_time)
{
    /**
    Could calculate how long it should take to get the results. For now use 2 seconds.
    */
    if (!collection_time)
    {
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    *collection_time = ADCS_CC_DEFAULT_COLLECTION_TIME;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool adcs_cc_set_midpoints(uint16_t new_midpoints[ADC_CC_COUNT])
{
    memcpy(cc_midpoints, new_midpoints, ADC_CC_COUNT * sizeof(cc_midpoints[0]));
    if (!persist_set_cc_midpoints(new_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


static bool _adcs_to_mV(uint16_t value, uint16_t* mV)
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

    if (inter_val > UINT16_MAX)
    {
        adc_debug("Cannot downsize value '%"PRIu32"'.", inter_val);
        return false;
    }

    *mV = (uint16_t)inter_val;
    return true;
}


static bool _adcs_current_clamp_conv(uint16_t adc_val, uint16_t* cc_mA, uint16_t midpoint)
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
    if (adc_val < midpoint)
    {
        adc_diff = midpoint - adc_val;
    }

    // Once the conversion is no longer linearly multiplicative this needs to be changed.
    if (!_adcs_to_mV(adc_diff, (uint16_t*)&inter_value))
    {
        adc_debug("Cannot get mV value of midpoint.");
        return false;
    }

    if (inter_value > UINT32_MAX / 2000)
    {
        adc_debug("Overflowing value.");
        return false;
    }
    inter_value *= 2000;
    inter_value /= 22;
    if (inter_value > UINT16_MAX)
    {
        adc_debug("Cannot downsize value '%"PRIu32"'.", inter_value);
        return false;
    }
    *cc_mA = (uint16_t)inter_value;
    return true;
}


static bool _adcs_find_active_channel_index(uint8_t* active_channel_index, uint8_t index)
{
    uint8_t adc_channel = adc_channel_array[index];
    for (unsigned i = 0; i < adc_cc_channels_active.len; i++)
    {
        if (adc_cc_channels_active.channels[i] == adc_channel)
        {
            *active_channel_index = i;
            return true;
        }
    }
    return false;
}


static bool _adcs_get_index(uint8_t* index, char* name)
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


static bool _adcs_get_channel(uint8_t* channel, uint8_t index)
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


bool adcs_cc_set_midpoint(uint16_t midpoint, char* name)
{
    uint8_t index;
    if (!_adcs_get_index(&index, name))
        return false;
    if (index > ADC_CC_COUNT)
    {
        return false;
    }
    cc_midpoints[index] = midpoint;
    if (!persist_set_cc_midpoints(cc_midpoints))
    {
        adc_debug("Could not set the persistent storage for the midpoint.");
        return false;
    }
    return true;
}


bool adcs_cc_get_midpoint(uint16_t* midpoint, char* name)
{
    uint8_t index;
    if (!_adcs_get_index(&index, name))
        return false;
    if (index > ADC_CC_COUNT)
        return false;
    *midpoint = cc_midpoints[index];
    return true;
}


measurements_sensor_state_t adcs_cc_begin(char* name)
{
    if (adcs_cc_running)
        return MEASUREMENTS_SENSOR_STATE_SUCCESS;

    if (!adc_cc_channels_active.len)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    if (!adcs_begin(adc_cc_channels_active.channels, adc_cc_channels_active.len))
        return MEASUREMENTS_SENSOR_STATE_BUSY;

    adc_debug("Started ADC reading for CC.");
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _adcs_cc_wait(void)
{
    adc_debug("Waiting for ADC CC");
    if (!adc_wait_done(CC_TIMEOUT_MS))
    {
        adc_debug("Timed out waiting for CC ADC.");
        return false;
    }
    return true;
}


bool adcs_cc_calibrate(void)
{
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    adcs_channels_active_t prev_adc_cc_channels_active = {0};
    memcpy(prev_adc_cc_channels_active.channels, adc_cc_channels_active.channels, adc_cc_channels_active.len * sizeof(adc_cc_channels_active.channels[0]));
    prev_adc_cc_channels_active.len = adc_cc_channels_active.len;

    memcpy(adc_cc_channels_active.channels, all_cc_channels, ADC_CC_COUNT);
    adc_cc_channels_active.len = ADC_CC_COUNT;

    if (adcs_cc_begin("") != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Could not begin the ADC.");
        return false;
    }
    if (!_adcs_cc_wait())
        return false;
    uint16_t midpoints[ADC_CC_COUNT];
    if (!adcs_collect_avgs(midpoints, ADC_CC_COUNT))
    {
        adc_debug("Could not average the ADC.");
        return false;
    }
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
        adc_debug("MP CC%u: %"PRIu16, i+1, midpoints[i]);
    adcs_cc_set_midpoints(midpoints);
    memcpy(adc_cc_channels_active.channels, prev_adc_cc_channels_active.channels, prev_adc_cc_channels_active.len);
    adc_cc_channels_active.len = prev_adc_cc_channels_active.len;
    return true;
}


measurements_sensor_state_t adcs_cc_get(char* name, value_t* value)
{
    if (adcs_cc_running)
    {
        adc_debug("ADCs not finished.");
        return MEASUREMENTS_SENSOR_STATE_BUSY;
    }

    uint8_t index, active_index;

    if (!_adcs_get_index(&index, name))
    {
        adc_debug("Cannot get index.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (!_adcs_find_active_channel_index(&active_index, index))
    {
        adc_debug("Not in active channel.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint16_t adcs_rms;
    uint16_t midpoint = cc_midpoints[index];

    if (!adcs_collect_rms(&adcs_rms, midpoint, adc_cc_channels_active.len, active_index))
    {
        adc_debug("Failed to get RMS");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint16_t cc_mA = 0;

    if (!_adcs_current_clamp_conv(adcs_rms, &cc_mA, midpoint))
    {
        adc_debug("Failed to get current clamp");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    *value = value_from_u16(cc_mA);
    adc_debug("CC = %umA", cc_mA);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


bool adcs_cc_get_blocking(char* name, value_t* value)
{
    uint8_t index;
    if (!_adcs_get_index(&index, name))
        return false;
    uint8_t channel;
    if (!_adcs_get_channel(&channel, index))
    {
        adc_debug("Could not get channel.");
        return false;
    }
    adcs_channels_active_t prev_active_channels;
    memcpy(prev_active_channels.channels, adc_cc_channels_active.channels, adc_cc_channels_active.len);
    prev_active_channels.len = adc_cc_channels_active.len;
    if (!adcs_cc_set_channels_active(&channel, 1))
    {
        adc_debug("Cannot set active channel.");
        return false;
    }
    if (adcs_cc_begin(name) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }
    if (!_adcs_cc_wait())
        return false;
    bool r = (adcs_cc_get(name, value) == MEASUREMENTS_SENSOR_STATE_SUCCESS);
    adcs_cc_set_channels_active(prev_active_channels.channels, prev_active_channels.len);
    return r;
}


bool adcs_cc_get_all_blocking(value_t* value_1, value_t* value_2, value_t* value_3)
{
    uint8_t all_cc_channels[ADC_CC_COUNT] = ADC_CC_CHANNELS;
    adcs_channels_active_t prev_adc_cc_channels_active = {0};
    memcpy(prev_adc_cc_channels_active.channels, adc_cc_channels_active.channels, adc_cc_channels_active.len * sizeof(adc_cc_channels_active.channels[0]));
    prev_adc_cc_channels_active.len = adc_cc_channels_active.len;

    if (!adcs_cc_set_channels_active(all_cc_channels, ADC_CC_COUNT))
    {
        adc_debug("Cannot set active channel.");
        return false;
    }

    if (adcs_cc_begin("") != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (!_adcs_cc_wait())
        return false;
    if (!adcs_cc_get(MEASUREMENTS_CURRENT_CLAMP_1_NAME, value_1) == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Couldnt get "MEASUREMENTS_CURRENT_CLAMP_1_NAME" value.");
        return false;
    }
    if (!adcs_cc_get(MEASUREMENTS_CURRENT_CLAMP_2_NAME, value_2) == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Couldnt get "MEASUREMENTS_CURRENT_CLAMP_2_NAME" value.");
        return false;
    }
    if (!adcs_cc_get(MEASUREMENTS_CURRENT_CLAMP_3_NAME, value_3) == MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Couldnt get "MEASUREMENTS_CURRENT_CLAMP_3_NAME" value.");
        return false;
    }

    return adcs_cc_set_channels_active(prev_adc_cc_channels_active.channels, prev_adc_cc_channels_active.len);
}


void cc_init(void)
{
    // Get the midpoints
    if (!persist_get_cc_midpoints(cc_midpoints))
    {
        // Assume it to be the theoretical midpoint
        adc_debug("Failed to load persistent midpoint.");
        uint16_t midpoints[ADC_CC_COUNT] = {ADC_MAX_VAL / 2};
        adcs_cc_set_midpoints(midpoints);
    }
}
