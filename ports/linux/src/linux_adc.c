#include <math.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <json-c/json.h>
#include <json-c/json_util.h>

#include "base_types.h"
#include "pinmap.h"
#include "log.h"
#include "adcs.h"
#include "linux.h"


#define ADCS_CONFIG_FILE                        LINUX_FILE_LOC"adcs_config.json"

#define ADCS_WAVE_TYPE_AC_STRING                "AC"
#define ADCS_WAVE_TYPE_DC_STRING                "DC"

#define ADCS_THEORETICAL_MIDPOINT               ((ADC_MAX_VAL + 1) / 2)
#define ADCS_5A_AMPLITUDE                       96.54f
#define ADCS_7_5A_AMPLITUDE                     144.81f
#define ADCS_10A_AMPLITUDE                      193.09f

#define ADCS_WAVE_AC_DEFAULT_AMPLITUDE          ADCS_5A_AMPLITUDE
#define ADCS_WAVE_AC_DEFAULT_AMPLITUDE_OFFSET   ADCS_THEORETICAL_MIDPOINT
#define ADCS_WAVE_AC_DEFAULT_FREQUENCY          50.f
#define ADCS_WAVE_AC_DEFAULT_PHASE              0.f

#define ADCS_WAVE_DC_DEFAULT_AMPLITUDE          ADCS_5A_AMPLITUDE
#define ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE   100

/*  */

#define ADCS_FTMA_4MA_AMPLITUDE                 744.860f
#define ADCS_FTMA_8MA_AMPLITUDE                 1487.718f
#define ADCS_FTMA_12MA_AMPLITUDE                2231.578f
#define ADCS_FTMA_16MA_AMPLITUDE                2975.437f
#define ADCS_FTMA_20MA_AMPLITUDE                3719.296f


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
static adcs_wave_t  _adcs_waves[ADC_COUNT]              = { {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=ADC_MAX_VAL,                    .random_amplitude=ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE } } ,     /* BAT_MON         */
                                                            {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_5A_AMPLITUDE,   .amplitude_offset=ADCS_WAVE_AC_DEFAULT_AMPLITUDE_OFFSET, .phase=0,        .frequency=ADCS_WAVE_AC_DEFAULT_FREQUENCY} }, /* CURRENT_CLAMP_1 */
                                                            {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_7_5A_AMPLITUDE, .amplitude_offset=ADCS_WAVE_AC_DEFAULT_AMPLITUDE_OFFSET, .phase=2*M_PI/3, .frequency=ADCS_WAVE_AC_DEFAULT_FREQUENCY} }, /* CURRENT_CLAMP_2 */
                                                            {.type=ADCS_WAVE_TYPE_AC, .ac={.amplitude=ADCS_10A_AMPLITUDE,  .amplitude_offset=ADCS_WAVE_AC_DEFAULT_AMPLITUDE_OFFSET, .phase=4*M_PI/3, .frequency=ADCS_WAVE_AC_DEFAULT_FREQUENCY} }, /* CURRENT_CLAMP_3 */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=ADCS_FTMA_4MA_AMPLITUDE,  .random_amplitude=ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE } } ,      /* FTMA_1       */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=ADCS_FTMA_8MA_AMPLITUDE,  .random_amplitude=ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE } } ,      /* FTMA_2       */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=ADCS_FTMA_16MA_AMPLITUDE, .random_amplitude=ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE } } ,      /* FTMA_3       */
                                                            {.type=ADCS_WAVE_TYPE_DC, .dc={.amplitude=ADCS_FTMA_20MA_AMPLITUDE, .random_amplitude=ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE } } };     /* FTMA_4       */


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
    adc_debug("Active:");
    for (uint8_t i = 0; i < _adcs_num_active_channels; i++)
        adc_debug("- %"PRIu8, _adcs_active_channels[i]);

    for (unsigned i = 0; i < _adcs_num_data; i++)
    {
        adcs_type_t* chan = (adcs_type_t*)&_adcs_active_channels[i % _adcs_num_active_channels];
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
            case ADCS_TYPE_FTMA1:
                wave = &_adcs_waves[ADC_INDEX_FTMA_1];
                break;
            case ADCS_TYPE_FTMA2:
                wave = &_adcs_waves[ADC_INDEX_FTMA_2];
                break;
            case ADCS_TYPE_FTMA3:
                wave = &_adcs_waves[ADC_INDEX_FTMA_3];
                break;
            case ADCS_TYPE_FTMA4:
                wave = &_adcs_waves[ADC_INDEX_FTMA_4];
                break;
            default:
                adc_debug("Fake ADC failed, unknown channel type.");
                continue;
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
                continue;
        }
    }
}


static bool _adcs_float_from_json(struct json_object * root, const char* name, float* val)
{
    if (!val)
        return false;
    struct json_object * val_obj = json_object_object_get(root, name);
    if (val_obj)
    {
        *val = json_object_get_double(val_obj);
        return true;
    }
    return false;
}


static bool _adcs_load_from_file(void)
{
    FILE* adcs_config_file = fopen(ADCS_CONFIG_FILE, "r");
    if (!adcs_config_file)
        return false;
    struct json_object * root = json_object_from_fd(fileno(adcs_config_file));
    if (!root)
    {
        fclose(adcs_config_file);
        return false;
    }
    for (unsigned i = 0; i < ADC_CC_COUNT; i++)
    {
        adcs_wave_t* wave = &_adcs_waves[i];
        if (!wave)
            continue;
        char name[5];
        snprintf(name, 4, "CC%u", i+1);
        struct json_object * cc = json_object_object_get(root, name);
        if (!cc)
            continue;
        struct json_object * type = json_object_object_get(cc, "type");
        if (!type)
            continue;
        const char * type_str = json_object_get_string(type);
        unsigned len = strlen(type_str);
        if (len != 2)
            continue;
        float v;
        if (strncmp(type_str, ADCS_WAVE_TYPE_AC_STRING, 2) == 0)
        {
            if (wave->type != ADCS_WAVE_TYPE_AC)
            {
                wave->type                = ADCS_WAVE_TYPE_AC;
                wave->ac.amplitude        = ADCS_WAVE_AC_DEFAULT_AMPLITUDE;
                wave->ac.amplitude_offset = ADCS_WAVE_AC_DEFAULT_AMPLITUDE_OFFSET;
                wave->ac.frequency        = ADCS_WAVE_AC_DEFAULT_FREQUENCY;
                wave->ac.phase            = ADCS_WAVE_AC_DEFAULT_PHASE;
            }

            if (_adcs_float_from_json(cc, "amplitude", &v))
                wave->ac.amplitude = v;

            if (_adcs_float_from_json(cc, "amplitude_offset", &v))
                wave->ac.amplitude_offset = v;

            if (_adcs_float_from_json(cc, "frequency", &v))
                wave->ac.frequency = v;

            if (_adcs_float_from_json(cc, "phase", &v))
                wave->ac.phase = v;
        }
        else if (strncmp(type_str, ADCS_WAVE_TYPE_DC_STRING, 2) == 0)
        {
            if (wave->type != ADCS_WAVE_TYPE_DC)
            {
                wave->type                = ADCS_WAVE_TYPE_DC;
                wave->dc.amplitude        = ADCS_WAVE_DC_DEFAULT_AMPLITUDE;
                wave->dc.random_amplitude = ADCS_WAVE_DC_DEFAULT_RANDOM_AMPLITUDE;
            }
            if (_adcs_float_from_json(cc, "amplitude", &v))
                wave->dc.amplitude = v;

            if (_adcs_float_from_json(cc, "random_amplitude", &v))
                wave->dc.random_amplitude = v;
        }
        else
            continue;
    }
    fclose(adcs_config_file);
    return true;
}


static void _adcs_remove_file(void)
{
    remove(ADCS_CONFIG_FILE);
}


void linux_adc_generate(void)
{
    if (_adcs_load_from_file())
        _adcs_remove_file();
    _adcs_fill_buffer();
    adcs_dma_complete();
}


void platform_setup_adc(adc_setup_config_t* config)
{
    _adcs_buf = (uint16_t*)config->mem_addr;
}


void platform_adc_set_regular_sequence(uint8_t num_channels, adcs_type_t* channels)
{
    _adcs_num_active_channels = num_channels;
    memcpy((adcs_type_t*)_adcs_active_channels, channels, sizeof(adcs_type_t) * num_channels);
}


void platform_adc_start_conversion_regular(void)
{
    linux_kick_adc_gen();
}


void platform_adc_power_off(void)
{
}


void platform_adc_set_num_data(unsigned num_data)
{
    _adcs_num_data = num_data;
}
