#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


#include "cc.h"

#include "common.h"
#include "log.h"
#include "adcs.h"
#include "persist_config.h"
#include "uart_rings.h"


#define CC_DEFAULT_COLLECTION_TIME          1000
#define CC_TIMEOUT_MS                       2000
#define CC_NUM_SAMPLES                      ADCS_NUM_SAMPLES

#define CC_RESISTOR_OHM                     22

#define CC_DEFAULT_EXT_MAX_MA               (100 * 1000)
#define CC_DEFAULT_INT_MAX_MV               50


typedef struct
{
    adcs_type_t active[ADC_CC_COUNT];
    unsigned    len;
} cc_active_clamps_t;


static adcs_type_t          _cc_adc_clamp_array[ADC_CC_COUNT]   = ADC_TYPES_ALL_CC;
static uint32_t             _cc_midpoints[ADC_CC_COUNT];
static cc_active_clamps_t   _cc_adc_active_clamps               = {0};
static adcs_type_t          _cc_running_isolated                = ADCS_TYPE_INVALID;
static bool                 _cc_running[ADC_CC_COUNT]           = {false};
static uint32_t             _cc_collection_time                 = CC_DEFAULT_COLLECTION_TIME;
static cc_config_t*         _configs;


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
    if (!adcs_to_mV(adc_diff, &inter_value))
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


static bool _cc_find_active_clamp_index(uint8_t* active_clamp_index, uint8_t index)
{
    uint8_t adc_clamp = _cc_adc_clamp_array[index];
    for (unsigned i = 0; i < _cc_adc_active_clamps.len; i++)
    {
        if (_cc_adc_active_clamps.active[i] == adc_clamp)
        {
            *active_clamp_index = i;
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


static bool _cc_get_clamp(adcs_type_t* clamp, uint8_t index)
{
    if (!clamp)
    {
        adc_debug("Handed NULL pointer.");
        return false;
    }
    switch (index)
    {
        case 0:
            *clamp = ADCS_TYPE_CC_CLAMP1;
            return true;
        case 1:
            *clamp = ADCS_TYPE_CC_CLAMP2;
            return true;
        case 2:
            *clamp = ADCS_TYPE_CC_CLAMP3;
            return true;
        default:
            adc_debug("No clamp for %"PRIu8, index);
            return false;
    }
}


static bool _cc_get_info(char* name, uint8_t* index, uint8_t* active_index, adcs_type_t* clamp)
{
    uint8_t index_local;
    if (!_cc_get_index(&index_local, name))
    {
        adc_debug("Cannot get index.");
        return false;
    }
    if (active_index &&
        !_cc_find_active_clamp_index(active_index, index_local))
    {
        adc_debug("Not in active clamp.");
        return false;
    }
    if (clamp &&
        !_cc_get_clamp(clamp, index_local))
    {
        adc_debug("Cannot get clamp.");
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


bool cc_set_active_clamps(adcs_type_t* active_clamps, unsigned len)
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
    memcpy(_cc_adc_active_clamps.active, active_clamps, len * sizeof(adcs_type_t));
    _cc_adc_active_clamps.len = len;
    adc_debug("Setting %"PRIu8" active clamps.", len);
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


static measurements_sensor_state_t _cc_get_collection_time(char* name, uint32_t* collection_time)
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


static measurements_sensor_state_t _cc_begin(char* name, bool in_isolation)
{
    uint8_t index;
    if (!_cc_get_index(&index, name))
    {
        adc_debug("Cannot get index.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    if (index >= ADC_CC_COUNT)
        return MEASUREMENTS_SENSOR_STATE_ERROR;

    adcs_resp_t resp;

    if (in_isolation)
    {
        adcs_type_t lone_cc = ADCS_TYPE_CC_CLAMP1 + index;

        for (unsigned i = 0; i < ADC_CC_COUNT; i++)
        {
            if (i == index)
                continue;
            if (_cc_running[i])
            {
                adc_debug("Cannot start CC in isolation when running.");
                return MEASUREMENTS_SENSOR_STATE_ERROR;
            }
        }

        _cc_running_isolated = lone_cc;

        resp = adcs_begin(&lone_cc, 1, CC_NUM_SAMPLES, ADCS_KEY_CC);
    }
    else
    {
        if (!_cc_adc_active_clamps.len)
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        if (_cc_running_isolated != ADCS_TYPE_INVALID)
        {
            adc_debug("Cannot start CC non isolated as isolation running.");
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        }

        for (unsigned i = 0; i < ADC_CC_COUNT; i++)
        {
            if (i == index)
                continue;
            if (_cc_running[i])
            {
                _cc_running[index] = true;
                return MEASUREMENTS_SENSOR_STATE_SUCCESS;
            }
        }
        resp = adcs_begin(_cc_adc_active_clamps.active, _cc_adc_active_clamps.len, CC_NUM_SAMPLES, ADCS_KEY_CC);
    }

    switch(resp)
    {
        case ADCS_RESP_FAIL:
            adc_debug("Failed to begin CC ADC.");
            if (_cc_running_isolated != ADCS_TYPE_INVALID)
                _cc_running_isolated = ADCS_TYPE_INVALID;
            return MEASUREMENTS_SENSOR_STATE_ERROR;
        case ADCS_RESP_WAIT:
            return MEASUREMENTS_SENSOR_STATE_BUSY;
        case ADCS_RESP_OK:
            break;
    }
    _cc_running[index] = true;
    adc_debug("Started ADC reading for CC.");
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_sensor_state_t _cc_get(char* name, measurements_reading_t* value)
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
    uint32_t midpoint = _configs[index].midpoint;

    unsigned cc_len;

    if (_cc_running_isolated != ADCS_TYPE_INVALID)
        cc_len = 1;
    else
        cc_len = _cc_adc_active_clamps.len;

    adcs_resp_t resp = adcs_collect_rms(&adcs_rms, midpoint, cc_len, CC_NUM_SAMPLES, active_index, ADCS_KEY_CC, &_cc_collection_time);

    _cc_running_isolated = ADCS_TYPE_INVALID;

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

    uint32_t scale_factor = _configs[index].ext_max_mA / _configs[index].int_max_mV;
    if (!_cc_conv(adcs_rms, &cc_mA, midpoint, scale_factor))
    {
        adc_debug("Failed to get current clamp");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }

    value->v_i64 = (int64_t)cc_mA;
    adc_debug("CC = %"PRIu32"mA", cc_mA);
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static bool _cc_set_midpoints(uint32_t new_midpoints[ADC_CC_COUNT])
{
    for (uint8_t i = 0; i < ADC_CC_COUNT; i++)
        _configs[i].midpoint = new_midpoints[i];
    return true;
}


static bool _cc_set_midpoint(uint32_t midpoint, char* name)
{
    uint8_t index;
    if (!_cc_get_index(&index, name))
        return false;
    if (index >= ADC_CC_COUNT)
    {
        return false;
    }
    _configs[index].midpoint = midpoint;
    return true;
}


static bool _cc_get_midpoint(uint32_t* midpoint, char* name)
{
    uint8_t index;
    if (!_cc_get_index(&index, name))
        return false;
    if (index >= ADC_CC_COUNT)
        return false;
    *midpoint = _cc_midpoints[index];
    return true;
}


static bool _cc_calibrate(void)
{
    adcs_type_t all_cc_clamps[ADC_CC_COUNT] = ADC_TYPES_ALL_CC;
    cc_active_clamps_t prev_cc_adc_active_clamps = {0};
    memcpy(prev_cc_adc_active_clamps.active, _cc_adc_active_clamps.active, _cc_adc_active_clamps.len * sizeof(_cc_adc_active_clamps.active[0]));
    prev_cc_adc_active_clamps.len = _cc_adc_active_clamps.len;

    memcpy(_cc_adc_active_clamps.active, all_cc_clamps, ADC_CC_COUNT);
    _cc_adc_active_clamps.len = ADC_CC_COUNT;

    adcs_resp_t resp = adcs_begin(_cc_adc_active_clamps.active, _cc_adc_active_clamps.len, CC_NUM_SAMPLES, ADCS_KEY_CC);
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
    _cc_set_midpoints(midpoints);
    memcpy(_cc_adc_active_clamps.active, prev_cc_adc_active_clamps.active, prev_cc_adc_active_clamps.len);
    _cc_adc_active_clamps.len = prev_cc_adc_active_clamps.len;
    return true;
}


bool cc_get_blocking(char* name, measurements_reading_t* value)
{
    if (_cc_begin(name, true) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }
    if (!_cc_wait())
        return false;
    bool r = (_cc_get(name, value) == MEASUREMENTS_SENSOR_STATE_SUCCESS);
    _cc_release_all();
    return r;
}


bool cc_get_all_blocking(measurements_reading_t* value_1, measurements_reading_t* value_2, measurements_reading_t* value_3)
{
    adcs_type_t all_cc_clamps[ADC_CC_COUNT] = ADC_TYPES_ALL_CC;
    cc_active_clamps_t prev_cc_adc_active_clamps = {0};
    memcpy(prev_cc_adc_active_clamps.active, _cc_adc_active_clamps.active, _cc_adc_active_clamps.len * sizeof(_cc_adc_active_clamps.active[0]));
    prev_cc_adc_active_clamps.len = _cc_adc_active_clamps.len;

    if (!cc_set_active_clamps(all_cc_clamps, ADC_CC_COUNT))
    {
        adc_debug("Cannot set active clamp.");
        return false;
    }

    if (_cc_begin(MEASUREMENTS_CURRENT_CLAMP_1_NAME, false) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (_cc_begin(MEASUREMENTS_CURRENT_CLAMP_2_NAME, false) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (_cc_begin(MEASUREMENTS_CURRENT_CLAMP_3_NAME, false) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        adc_debug("Can not begin ADC.");
        return false;
    }

    if (!_cc_wait())
        return false;
    char name[MEASURE_NAME_NULLED_LEN] = {0};
    if (_cc_get(MEASUREMENTS_CURRENT_CLAMP_1_NAME, value_1) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_1_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    if (_cc_get(MEASUREMENTS_CURRENT_CLAMP_2_NAME, value_2) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_2_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    if (_cc_get(MEASUREMENTS_CURRENT_CLAMP_3_NAME, value_3) != MEASUREMENTS_SENSOR_STATE_SUCCESS)
    {
        strncpy(name, MEASUREMENTS_CURRENT_CLAMP_3_NAME, MEASURE_NAME_NULLED_LEN);
        goto bad_exit;
    }
    _cc_release_all();

    return cc_set_active_clamps(prev_cc_adc_active_clamps.active, prev_cc_adc_active_clamps.len);
bad_exit:
    _cc_release_all();
    adc_debug("Couldnt get %s value.", name);
    return false;
}


static void  _cc_enable(char* name, bool enabled)
{
    uint8_t index_local;
    if (!_cc_get_index(&index_local, name))
    {
        adc_debug("Cannot get index.");
        return;
    }

    adcs_type_t clamps[ADC_CC_COUNT] = {0};
    unsigned len = 0;

    adcs_type_t target_cc = ADCS_TYPE_CC_CLAMP1 + index_local;

    if (enabled)
    {
        for (unsigned i = 0; i < _cc_adc_active_clamps.len; i++)
            if (_cc_adc_active_clamps.active[i] < target_cc)
                clamps[len++] = _cc_adc_active_clamps.active[i];

        clamps[len++] = target_cc;

        for (unsigned i = 0; i < _cc_adc_active_clamps.len; i++)
            if (_cc_adc_active_clamps.active[i] > target_cc)
                clamps[len++] = _cc_adc_active_clamps.active[i];
    }
    else
    {
        for (unsigned i = 0; i < _cc_adc_active_clamps.len; i++)
            if (_cc_adc_active_clamps.active[i] != target_cc)
                clamps[len++] = _cc_adc_active_clamps.active[i];
    }

    cc_set_active_clamps(clamps, len);
}


static measurements_value_type_t _cc_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void cc_inf_init(measurements_inf_t* inf)
{
    inf->collection_time_cb = _cc_get_collection_time;
    inf->init_cb            = _cc_begin;
    inf->get_cb             = _cc_get;
    inf->enable_cb          = _cc_enable;
    inf->value_type_cb      = _cc_value_type;
}


void cc_setup_default_mem(cc_config_t* memory, unsigned size)
{
    uint8_t num_cc_configs = ADC_CC_COUNT;
    if (sizeof(cc_config_t) * ADC_CC_COUNT > size)
    {
        log_error("CC config is larger than the size of memory given.");
        num_cc_configs = size / sizeof(cc_config_t);
    }
    for (uint8_t i = 0; i < num_cc_configs; i++)
    {
        memory[i].midpoint      = CC_DEFAULT_MIDPOINT;
        memory[i].ext_max_mA    = CC_DEFAULT_EXT_MAX_MA;
        memory[i].int_max_mV    = CC_DEFAULT_INT_MAX_MV;
    }
}


void cc_init(void)
{
    _configs = persist_data.model_config.cc_configs;
    if (!_configs)
    {
        adc_debug("Failed to load persistent midpoint.");
        static cc_config_t default_configs[ADC_CC_COUNT];
        cc_setup_default_mem(default_configs, sizeof(cc_config_t) * ADC_CC_COUNT);
        _configs = default_configs;
    }
}


static void cc_cb(char* args)
{
    char* p;
    uint8_t cc_num = strtoul(args, &p, 10);
    measurements_reading_t value_1;
    if (p == args)
    {
        measurements_reading_t value_2, value_3;
        if (!cc_get_all_blocking(&value_1, &value_2, &value_3))
        {
            log_out("Could not get CC values.");
            return;
        }
        log_out("CC1 = %"PRIi64".%"PRIi64" A", value_1.v_i64/1000, value_1.v_i64%1000);
        log_out("CC2 = %"PRIi64".%"PRIi64" A", value_2.v_i64/1000, value_2.v_i64%1000);
        log_out("CC3 = %"PRIi64".%"PRIi64" A", value_3.v_i64/1000, value_3.v_i64%1000);
        return;
    }
    if (cc_num > 3 || cc_num == 0)
    {
        log_out("cc [1/2/3]");
        return;
    }
    char name[4];
    snprintf(name, 4, "CC%"PRIu8, cc_num);
    if (!cc_get_blocking(name, &value_1))
    {
        log_out("Could not get adc value.");
        return;
    }

    log_out("CC = %"PRIi64"mA", value_1.v_i64);
}


static void cc_calibrate_cb(char *args)
{
    _cc_calibrate();
}


static void cc_mp_cb(char* args)
{
    // 2046 CC1
    char* p;
    float new_mp = strtof(args, &p);
    p = skip_space(p);
    uint32_t new_mp32;
    if (p == args)
    {
        _cc_get_midpoint(&new_mp32, p);
        log_out("MP: %"PRIu32".%03"PRIu32, new_mp32/1000, new_mp32%1000);
        return;
    }
    new_mp32 = new_mp * 1000;
    p = skip_space(p);
    if (!_cc_set_midpoint(new_mp32, p))
        log_out("Failed to set the midpoint.");
}


static void cc_gain(char* args)
{
    // <index> <ext_A> <int_mV>
    // 1       100     50
    char* p;
    uint8_t index = strtoul(args, &p, 10);
    p = skip_space(p);
    if (strlen(p) == 0)
    {
        for (uint8_t i = 0; i < ADC_CC_COUNT; i++)
        {
            log_out("CC%"PRIu8" EXT max: %"PRIu32".%03"PRIu32"A", i+1, _configs[i].ext_max_mA/1000, _configs[i].ext_max_mA%1000);
            log_out("CC%"PRIu8" INT max: %"PRIu32".%03"PRIu32"V", i+1, _configs[i].int_max_mV/1000, _configs[i].int_max_mV%1000);
        }
        return;
    }
    if (index == 0 || index > ADC_CC_COUNT + 1 || p == args)
        goto syntax_exit;
    index--;

    char* q;
    float ext_A = strtof(p, &q);
    if (q == p)
        goto print_exit;
    q = skip_space(q);
    uint32_t int_mA = strtoul(q, &p, 10);
    if (p == q)
        goto syntax_exit;
    _configs[index].ext_max_mA = ext_A * 1000;
    _configs[index].int_max_mV = int_mA;
    log_out("Set the CC gain:");
print_exit:
    log_out("EXT max: %"PRIu32".%03"PRIu32"A", _configs[index].ext_max_mA/1000, _configs[index].ext_max_mA%1000);
    log_out("INT max: %"PRIu32".%03"PRIu32"V", _configs[index].int_max_mV/1000, _configs[index].int_max_mV%1000);
    return;
syntax_exit:
    log_out("Syntax: cc_gain <channel> <ext max A> <ext min mV>");
    log_out("e.g cc_gain 3 100 50");
}


struct cmd_link_t* cc_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "cc",           "CC value",                 cc_cb                         , false , NULL },
                                       { "cc_cal",       "Calibrate the cc",         cc_calibrate_cb               , false , NULL },
                                       { "cc_mp",        "Set the CC midpoint",      cc_mp_cb                      , false , NULL },
                                       { "cc_gain",      "Set the max int and ext",  cc_gain                       , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
