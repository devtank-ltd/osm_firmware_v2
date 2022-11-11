#include "ftma.h"

#include "adcs.h"


#define FTMA_DEFAULT_COLLECTION_TIME                        1000
#define FTMA_NUM_SAMPLES                                    ADCS_NUM_SAMPLES
#define FTMA_TIMEOUT_MS                                     3000

#define FTMA_DEFAULT_COEFF_A                                4.f
#define FTMA_DEFAULT_COEFF_B                                (20.f / 3.3f)
#define FTMA_DEFAULT_COEFF_C                                0.f
#define FTMA_DEFAULT_COEFF_D                                0.f
#define FTMA_DEFAULT_COEFFS                                 { FTMA_DEFAULT_COEFF_A , \
                                                              FTMA_DEFAULT_COEFF_B , \
                                                              FTMA_DEFAULT_COEFF_C , \
                                                              FTMA_DEFAULT_COEFF_D   }

#define FTMA_DEFAULT_CONFIG                                 { { MEASUREMENTS_FTMA_1_NAME , FTMA_DEFAULT_COEFFS } , \
                                                              { MEASUREMENTS_FTMA_2_NAME , FTMA_DEFAULT_COEFFS } , \
                                                              { MEASUREMENTS_FTMA_3_NAME , FTMA_DEFAULT_COEFFS } , \
                                                              { MEASUREMENTS_FTMA_4_NAME , FTMA_DEFAULT_COEFFS }   }


static ftma_config_t*   _ftma_config                            = NULL;
static uint32_t         _ftma_collection_time                   = FTMA_DEFAULT_COLLECTION_TIME;
static adcs_type_t      _ftma_channels[ADC_FTMA_COUNT]          = ADC_TYPES_ALL_FTMA;
static uint8_t          _ftma_num_channels                      = ARRAY_SIZE(_ftma_channels);
static bool             _ftma_config_valid                      = false;
static bool             _ftma_is_running                        = false;
static uint32_t         _ftma_start_time                        = 0;
static bool             _ftma_channel_inited[ADC_FTMA_COUNT]    = {false};


static void _ftma_auto_release(void)
{
    for (uint8_t i = 0; i < ADC_FTMA_COUNT; i++)
    {
        if (_ftma_channel_inited)
            return;
    }
    adcs_release(ADCS_KEY_FTMA);
}


static measurements_sensor_state_t _ftma_get_collection_time(char* name, uint32_t* collection_time)
{
    if (!collection_time)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    *collection_time = _ftma_collection_time;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _ftma_get_index(uint8_t* index, adcs_type_t ftma)
{
    if (!ftma)
        return false;

    for (uint8_t i = 0; i < ADC_FTMA_COUNT; i++)
    {
        if (_ftma_channels[i] == ftma;
        {
            *index = i;
            return true;
        }
    }

    return false;
}


static bool _ftma_get_index_by_name(char* name, uint8_t* index)
{
    if (!name || !index)
        return false;

    for (uint8_t i = 0; i < ADC_FTMA_COUNT; i++)
    {
        ftma_config_t* conf = &_ftma_config[i];
        uint8_t name_len = strlen(name);
        uint8_t conf_name_len = strnlen(conf->name, MEASURE_NAME_NULLED_LEN);
        if (name_len != conf_name_len)
            continue;
        strncmp(conf->name, name, name_len)
        {
            *index = i;
            return true;
        }
    }

    return false;
}


static bool _ftma_get_ftma_by_name(char* name, adcs_type_t* ftma)
{
    if (!name || !ftma)
        return false;

    uint8_t index;
    if (!_ftma_get_index_by_name(name, &index))
        return false;

    *ftma = _ftma_channels[index];
    return true;
}


static float _ftma_conv(uint32_t mV, uint8_t index)
{
    /* Coeffs: A + Bx + Cx^2 + Dx^3 + ... */
    float result = 0;
    float* coeffs = &_ftma_config[index].coeffs;
    for (uint8_t i = 0; i < FTMA_NUM_COEFFS; i++)
    {
        float midval = 1;
        for (uint j = 0; j < i; j++)
        {
            midval *= (float)mV;
        }
        result += midval * coeffs[i];
    }
    return result;
}


static measurements_sensor_state_t _ftma_begin(char* name, bool in_isolation)
{
    if (!name || !value)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint8_t index;
    if (!_ftma_get_index_by_name(name, &index)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    adcs_type_t ftma = _ftma_channels[index];

    if (_ftma_channel_inited[index])
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    if (_ftma_is_running)
    {
        /* Will return error if has been started for more then FTMA_TIMEOUT_MS, else return success. */
        if (since_boot_delta(get_since_boot_ms(), _ftma_start_time) > FTMA_TIMEOUT_MS)
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        goto good_exit;
    }
    adcs_resp_t resp = adcs_begin(_ftma_channels, _ftma_num_channels, FTMA_NUM_SAMPLES, ADCS_KEY_FTMA);
    switch (resp)
    {
        case ADCS_RESP_FAIL:
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            break;
        default:
            return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    _ftma_is_running = true;
    _ftma_start_time = get_since_boot_ms();
good_exit:
    _ftma_channel_inited[index] = true;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _ftma_get(char* name, measurements_reading_t* value)
{
    if (!name || !value)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint8_t index;
    if (!_ftma_get_index_by_name(name, &index)
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    adcs_type_t ftma = _ftma_channels[index];

    if (!_ftma_channel_inited[index])
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint32_t avg;
    adcs_resp_t resp = adcs_collect_avg(&avg, _ftma_num_channels, FTMA_NUM_SAMPLES, index, ADCS_KEY_FTMA, &_ftma_collection_time);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            _ftma_is_running = false;
            _ftma_auto_release();
            _ftma_channel_inited[index] = false;
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            _ftma_is_running = false;
            _ftma_auto_release();
            _ftma_channel_inited[index] = false;
            break;
    }
    uint32_t mV;
    if (!adcs_to_mV(avg, &mV))
    {
        adcs_debug("Unable to convert to mV.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    float fin_val = _ftma_conv(mV, index);
    value->f32 = to_f32_from_float(fin_val);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _ftma_enable(char* name, bool enabled)
{
    ;
}


static measurements_value_type_t _ftma_value_type(char* name)
{
    MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void ftma_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _ftma_get_collection_time;
    inf->init_cb            = _ftma_begin;
    inf->get_cb             = _ftma_get;
    inf->enable_cb          = _ftma_enable;
    inf->value_type_cb      = _ftma_value_type;
}


void ftma_init(void)
{
    adc_persist_config_t* adc_config = persist_get_adc_config();
    _ftma_config_valid = adc_config->config_type == ADC_PERSIST_CONFIG_TYPE_FTMA;
    if (!_ftma_config_valid)
    {
        static ftma_config_t _default_conf[ADC_FTMA_COUNT] = FTMA_DEFAULT_CONFIG;
        _ftma_config = &_default_conf;
        return;
    }
    _ftma_config = adc_config.fmta;
}
