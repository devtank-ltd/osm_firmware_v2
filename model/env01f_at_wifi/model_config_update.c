#include "model_config.h"


static void _model_config_update_data_v3_to_v4(persist_model_config_v3_t* v3, persist_model_config_v4_t* v4)
{
    memset(v4, 0, sizeof(persist_model_config_v4_t));
    v4->mins_interval = v3->mins_interval;
    memcpy(&v4->modbus_bus, &v3->modbus_bus, sizeof(modbus_bus_t));
    memcpy(&v4->comms_config, &v3->comms_config, sizeof(comms_config_t));
    memcpy(v4->cc_configs, v3->cc_configs, sizeof(cc_config_t) * ADC_CC_COUNT);
    memcpy(v4->ios_state, v3->ios_state, sizeof(uint16_t) * IOS_COUNT);
    memcpy(v4->sai_cal_coeffs, v3->sai_cal_coeffs, sizeof(float) * SAI_NUM_CAL_COEFFS);
    v4->sai_no_buf = v3->sai_no_buf;
    for (unsigned i = 0; i < W1_PULSE_COUNT; i++)
    {
        v4->pulsecount_debounces_ms[i] = 50;
    }
}


bool model_config_update(void* from_config, persist_model_config_t* to_config, uint16_t from_model_version)
{
    if (3 == from_model_version)
    {
        persist_model_config_v3_t* v3 = (persist_model_config_v3_t*)from_config;
        _model_config_update_data_v3_to_v4(v3, to_config);
        return true;
    }
    return false;
}
