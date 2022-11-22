#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "ftma.h"

#include "adcs.h"
#include "common.h"
#include "log.h"
#include "persist_config.h"


#define FTMA_DEFAULT_COLLECTION_TIME                        1000
#define FTMA_NUM_SAMPLES                                    ADCS_NUM_SAMPLES
#define FTMA_TIMEOUT_MS                                     3000

#define FTMA_MIN_MA                                         4.f
#define FTMA_MAX_MA                                         20.f

#define FTMA_DEFAULT_COEFF_A                                FTMA_MIN_MA
#define FTMA_DEFAULT_COEFF_B                                ((FTMA_MAX_MA - FTMA_DEFAULT_COEFF_A) / ADC_MAX_MV)
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
static bool             _ftma_is_running                        = false;
static uint32_t         _ftma_start_time                        = 0;
static bool             _ftma_channel_inited[ADC_FTMA_COUNT]    = {false};


static void _ftma_auto_release(void)
{
    for (uint8_t i = 0; i < ADC_FTMA_COUNT; i++)
    {
        if (_ftma_channel_inited[i])
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
        if (strncmp(conf->name, name, name_len) == 0)
        {
            *index = i;
            return true;
        }
    }

    return false;
}


static float _ftma_conv(uint32_t mV, uint8_t index)
{
    /* Coeffs: A + Bx + Cx^2 + Dx^3 + ... */
    float result = 0;
    float* coeffs = _ftma_config[index].coeffs;
    for (uint8_t i = 0; i < FTMA_NUM_COEFFS; i++)
    {
        float midval = 1;
        for (uint8_t j = 0; j < i; j++)
        {
            midval *= (float)mV / 1000.f;
        }
        result += midval * coeffs[i];
    }
    return result;
}


static measurements_sensor_state_t _ftma_begin(char* name, bool in_isolation)
{
    if (!name)
    {
        adc_debug("Handed a NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    uint8_t index;
    if (!_ftma_get_index_by_name(name, &index))
    {
        adc_debug("'%s' does not match any FTMAs.", name);
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (_ftma_channel_inited[index])
    {
        adc_debug("FTMA is already inited.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (_ftma_is_running)
    {
        /* Will return error if has been started for more then FTMA_TIMEOUT_MS, else return success. */
        if (since_boot_delta(get_since_boot_ms(), _ftma_start_time) > FTMA_TIMEOUT_MS)
        {
            adc_debug("ADC been running too long.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        }
        goto good_exit;
    }
    adcs_resp_t resp = adcs_begin(_ftma_channels, _ftma_num_channels, FTMA_NUM_SAMPLES, ADCS_KEY_FTMA);
    switch (resp)
    {
        case ADCS_RESP_FAIL:
        {
            adc_debug("ADC begin failed.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        }
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            break;
        default:
        {
            adc_debug("Unexpected response from ADC.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        }
    }
    _ftma_is_running = true;
    _ftma_start_time = get_since_boot_ms();
    adc_debug("ADC successfully started for FTMA.");
good_exit:
    _ftma_channel_inited[index] = true;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _ftma_get(char* name, measurements_reading_t* value)
{
    if (!name || !value)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint8_t index;
    if (!_ftma_get_index_by_name(name, &index))
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    if (!_ftma_channel_inited[index])
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    uint32_t avg;
    adcs_resp_t resp = adcs_collect_avg(&avg, _ftma_num_channels, FTMA_NUM_SAMPLES, index, ADCS_KEY_FTMA, &_ftma_collection_time);
    switch(resp)
    {
        case ADCS_RESP_FAIL:
            _ftma_is_running = false;
            _ftma_channel_inited[index] = false;
            _ftma_auto_release();
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            _ftma_is_running = false;
            _ftma_channel_inited[index] = false;
            _ftma_auto_release();
            break;
    }
    adc_debug("FTMA Raw: %"PRIu32, avg);
    uint32_t mV;
    if (!adcs_to_mV(avg, &mV))
    {
        adc_debug("Unable to convert to mV.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    adc_debug("FTMA: %"PRIu32" mV", mV);
    float fin_val = _ftma_conv(mV, index);
    value->v_f32 = to_f32_from_float(fin_val);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static void _ftma_enable(char* name, bool enabled)
{
    ;
}


static measurements_value_type_t _ftma_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_FLOAT;
}


void ftma_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _ftma_get_collection_time;
    inf->init_cb            = _ftma_begin;
    inf->get_cb             = _ftma_get;
    inf->enable_cb          = _ftma_enable;
    inf->value_type_cb      = _ftma_value_type;
}

void ftma_setup_default_mem(ftma_config_t* memory, unsigned size)
{
    uint8_t num_ftma_configs = ADC_FTMA_COUNT;
    if (sizeof(ftma_config_t) * ADC_FTMA_COUNT > size)
    {
        log_error("FTMA config is larger than the size of memory given.");
        num_ftma_configs = size / sizeof(ftma_config_t);
    }
    float default_coeffs[FTMA_NUM_COEFFS] = FTMA_DEFAULT_COEFFS;
    for (uint8_t i = 0; i < num_ftma_configs; i++)
    {
        char name[MEASURE_NAME_NULLED_LEN+1];
        snprintf(name, MEASURE_NAME_NULLED_LEN, "FTA%"PRIu8, i+1);
        strncpy(memory[i].name, name, MEASURE_NAME_LEN);
        memcpy(memory[i].coeffs, default_coeffs, sizeof(float) * FTMA_NUM_COEFFS);
    }
}


void ftma_init(void)
{
    _ftma_config = persist_data.model_config.ftma_configs;
    if (!_ftma_config)
    {
        static ftma_config_t _default_conf[ADC_FTMA_COUNT];
        ftma_setup_default_mem(_default_conf, sizeof(ftma_config_t) * ADC_FTMA_COUNT);
        _ftma_config = _default_conf;
    }
}


static void _ftma_name_cb(char* args)
{
    /* <original_name> <new_name>
     *      FTA1          TMP9
     */
    char* new_name = skip_to_space(args);
    *new_name = '\0';
    new_name++;
    char* orig_name = args;

    uint8_t index;
    if (!_ftma_get_index_by_name(orig_name, &index))
    {
        log_out("Failed to get FTMA with name '%s'.", orig_name);
        return;
    }
    if (index > ADC_FTMA_COUNT)
    {
        log_out("Index is out of range.");
        return;
    }
    unsigned new_len = strlen(new_name);
    if (new_len > MEASURE_NAME_LEN)
    {
        log_out("Max name length is %d, you tried length %u", MEASURE_NAME_LEN, new_len);
        return;
    }
    if (!measurements_rename(orig_name, new_name))
    {
        log_out("Failed to rename the measurement.");
        return;
    }
    strncpy(_ftma_config[index].name, new_name, MEASURE_NAME_LEN);
    log_out("Measurement '%s' is now called '%s'.", orig_name, new_name);
}


static void _ftma_coeff_cb(char* args)
{
    ;
}


struct cmd_link_t* ftma_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "ftma_name",   "Set the FTMA name",            _ftma_name_cb   , false , NULL },
                                       { "ftma_coeff",  "Set the FTMA coefficients",    _ftma_coeff_cb  , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
