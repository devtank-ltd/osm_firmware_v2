#include <stdint.h>

#include <osm/core/common.h>
#include "model_config.h"

#define MAX_MINMAX_COUNT 12

typedef struct
{
    uint32_t total_value; /* Max frequency of 1Mhz for 12 bit ADC*/

    uint32_t count:23;
    uint32_t average_limits:1;
    uint32_t max_min_index:4; /* Max number of the mins. */
    uint32_t min_max_index:4; /* Min number of the maxs. */

    uint16_t max_values[MAX_MINMAX_COUNT];
    uint16_t min_values[MAX_MINMAX_COUNT];
} adc_channel_info_t;


static volatile adc_channel_info_t _adc_channel_info[ADCS_CHANNEL_COUNT] = {{0}};
static volatile adc_channel_info_t _adc_channel_info_cur[ADCS_CHANNEL_COUNT] = {{0}};

static volatile uint32_t _collection_ms = 1000;


static void _setup_lowest_max(volatile adc_channel_info_t * channel_info)
{
    uint16_t lowest_value = channel_info->max_values[0];
    channel_info->min_max_index = 0;
    for(unsigned n = 1; n < MAX_MINMAX_COUNT; n++)
    {
        if (lowest_value > channel_info->max_values[n])
        {
            lowest_value = channel_info->max_values[n];
            channel_info->min_max_index = n;
        }
    }
}


static void _setup_largest_min(volatile adc_channel_info_t * channel_info)
{
    uint16_t largest_value = channel_info->min_values[0];
    channel_info->max_min_index = 0;
    for(unsigned n = 1; n < MAX_MINMAX_COUNT; n++)
    {
        if (largest_value < channel_info->min_values[n])
        {
            largest_value = channel_info->min_values[n];
            channel_info->max_min_index = n;
        }
    }
}


void osm_adc_loop_channel_add(unsigned channel, uint16_t adc)
{
    volatile adc_channel_info_t * channel_info = &_adc_channel_info[channel];

    if (channel_info->average_limits)
    {
        if (channel_info->count >= MAX_MINMAX_COUNT)
        {
            if (adc > channel_info->max_values[channel_info->min_max_index])
            {
                channel_info->max_values[channel_info->min_max_index] = adc;
                _setup_lowest_max(channel_info);
            }
            else if (adc < channel_info->min_values[channel_info->max_min_index])
            {
                channel_info->min_values[channel_info->max_min_index] = adc;
                _setup_largest_min(channel_info);
            }
        }
        else
        {
            channel_info->max_values[channel_info->count] = adc;
            channel_info->min_values[channel_info->count] = adc;
        }
    }
    else
    {
        if (adc < channel_info->min_values[0])
            channel_info->min_values[0] = adc;
        else if (adc > channel_info->max_values[0])
            channel_info->max_values[0] = adc;
    }

    channel_info->total_value += adc;
    channel_info->count ++;

    if (channel_info->average_limits && channel_info->count == MAX_MINMAX_COUNT)
    {
        _setup_lowest_max(channel_info);
        _setup_largest_min(channel_info);
    }
}


void osm_adc_loop_interate(void)
{
    static uint64_t last = 0;
    uint64_t now = osm_get_since_boot_ms();

    if ((now - last) < _collection_ms)
        return;

    for(unsigned n = 0; n < ADCS_CHANNEL_COUNT; n++)
    {
        volatile adc_channel_info_t * channel_info = &_adc_channel_info[n];

        _adc_channel_info_cur[n] = *channel_info;

        if (!channel_info->average_limits)
        {
            channel_info->max_values[0] = 0;
            channel_info->min_values[0] = 0xFFFF;
        }

        channel_info->total_value  = 0;
        channel_info->count = 0;
    }
}


bool     adc_enable_averaged_limits(unsigned adc, bool enable)
{
    if (adc >= ADCS_CHANNEL_COUNT)
        return false;

    _adc_channel_info[adc].average_limits = enable;
    return true;
}
