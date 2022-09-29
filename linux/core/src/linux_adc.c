#include <math.h>
#include <stddef.h>
#include <string.h>

#include "base_types.h"
#include "pinmap.h"
#include "log.h"
#include "adcs.h"


#define ADCS_THEORETICAL_MIDPOINT           ((ADC_MAX_VAL + 1) / 2)
#define ADCS_5A_AMPLITUDE                   96.54f
#define ADCS_7_5A_AMPLITUDE                 144.81f
#define ADCS_10A_AMPLITUDE                  193.09f


typedef enum
{
    ADCS_WAVE_TYPE_DC,
    ADCS_WAVE_TYPE_AC,
} adcs_wave_type_t;


typedef struct
{
    adcs_wave_type_t type;
    union
    {
        struct
        {
            float amplitude;
            float amplitude_offset;
            float phase;
            float frequency;
        } ac;
        struct
        {
            float amplitude;
            float random_amplitude;
        } dc;
    };
} adcs_wave_t;


static uint16_t*    _adcs_buf                           = NULL;         /* sizeof ADCS_NUM_SAMPLES */
static unsigned     _adcs_num_data                      = 0;
static uint8_t      _adcs_num_active_channels           = 0;
static adcs_type_t  _adcs_active_channels[ADC_COUNT]    = {0};
static adcs_wave_t  _adcs_waves[ADC_COUNT]              = { {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_5A_AMPLITUDE,   .amplitude_offset=ADCS_THEORETICAL_MIDPOINT, .phase=0,        .frequency=50} }, /* CURRENT_CLAMP_1 */
                                                            {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_7_5A_AMPLITUDE, .amplitude_offset=ADCS_THEORETICAL_MIDPOINT, .phase=2*M_PI/3, .frequency=50} }, /* CURRENT_CLAMP_2 */
                                                            {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_10A_AMPLITUDE,  .amplitude_offset=ADCS_THEORETICAL_MIDPOINT, .phase=4*M_PI/3, .frequency=50} }, /* CURRENT_CLAMP_3 */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=4095, .random_amplitude=100 } } ,    /* BAT_MON         */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=3000, .random_amplitude=100 } } ,    /* 3V3_RAIL_MON    */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=3000, .random_amplitude=100 } } };  /* 5V_RAIL_MON     */


static uint16_t _adcs_calculate_dc_wave(adcs_wave_t* wave)
{
    return wave->dc.amplitude;
}


static uint16_t _adcs_calculate_ac_wave(adcs_wave_t* wave, float x)
{
    float k = wave->ac.frequency * 2 * M_PI;
    return (wave->ac.amplitude * sin(k * x + wave->ac.phase) + wave->ac.amplitude_offset);
}


static void _adcs_fill_buffer(void)
{
    for (unsigned i = 0; i < _adcs_num_data; i++)
    {
        adcs_type_t* chan = &_adcs_active_channels[i % _adcs_num_active_channels];
        adcs_wave_t* wave;
        switch(*chan)
        {
            case ADCS_TYPE_BAT:
                wave = &_adcs_waves[ADC_INDEX_BAT_MON];
                break;
            case ADCS_TYPE_CC_CLAMP1:
                wave = &_adcs_waves[ADC_INDEX_CURRENT_CLAMP_1];
                break;
            case ADCS_TYPE_CC_CLAMP2:
                wave = &_adcs_waves[ADC_INDEX_CURRENT_CLAMP_2];
                break;
            case ADCS_TYPE_CC_CLAMP3:
                wave = &_adcs_waves[ADC_INDEX_CURRENT_CLAMP_3];
                break;
            default:
                adc_debug("Fake ADC failed, unknown channel type.");
                break;
        }
        switch(wave->type)
        {
            case ADCS_WAVE_TYPE_AC:
                _adcs_buf[i] = _adcs_calculate_ac_wave(wave, ((float)i) / _adcs_num_data);
                break;
            case ADCS_WAVE_TYPE_DC:
                _adcs_buf[i] = _adcs_calculate_dc_wave(wave);
                break;
            default:
                adc_debug("Fake ADC failed, unknown wave type.");
                break;
        }
    }
}


void platform_setup_adc(adc_setup_config_t* config)
{
    _adcs_buf = (uint16_t*)config->mem_addr;
}


void platform_adc_set_regular_sequence(uint8_t num_channels, adcs_type_t* channels)
{
    _adcs_num_active_channels = num_channels;
    memcpy(_adcs_active_channels, channels, sizeof(adcs_type_t) * num_channels);
}


void platform_adc_start_conversion_regular(void)
{
    _adcs_fill_buffer();
    adcs_dma_complete();
}


void platform_adc_power_off(void)
{
}


void platform_adc_set_num_data(unsigned num_data)
{
    _adcs_num_data = num_data;
}
