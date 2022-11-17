#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "timers.h"
#include "persist_config_header.h"
#include "platform.h"
#include "common.h"
#include "adcs.h"


void persistent_init(void)
{
    if (!persistent_base_init())
    {
        persist_data.mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
        adcs_setup_default_mem(&persist_data.adc_persist_config, sizeof(adc_persist_config_t));
    }
}


comms_config_t* persist_get_comms_config(void)
{
    return &persist_data.comms_config;
}


measurements_def_t * persist_get_measurements(void)
{
    return persist_measurements.measurements_arr;
}


bool persist_get_mins_interval(uint32_t * mins_interval)
{
    if (!mins_interval || !persist_data_valid)
    {
        return false;
    }
    *mins_interval = persist_data.mins_interval;
    return true;
}


bool persist_set_mins_interval(uint32_t mins_interval)
{
    if (!persist_data_valid)
    {
        return false;
    }
    persist_data.mins_interval = mins_interval;
    persist_commit();
    return true;
}


uint16_t *  persist_get_ios_state(void)
{
    return (uint16_t*)persist_data.ios_state;
}


modbus_bus_t * persist_get_modbus_bus(void)
{
    return &persist_data.modbus_bus;
}


float* persist_get_sai_cal_coeffs(void)
{
    return persist_data.sai_cal_coeffs;
}


adc_persist_config_t* persist_get_adc_config(void)
{
    if (!persist_data_valid)
        return NULL;
    return &persist_data.adc_persist_config;
}
