#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "timers.h"
#include "persist_config_header.h"
#include "platform.h"

static bool                             persist_data_valid = false;
static persist_storage_t                persist_data __attribute__((aligned (16)));
static persist_measurements_storage_t   persist_measurements __attribute__((aligned (16)));

void persistent_init(void)
{
    persist_storage_t* persist_data_raw = platform_get_raw_persist();
    persist_measurements_storage_t* persist_measurements_raw = platform_get_measurements_raw_persist();

    uint32_t vs = persist_data_raw->version;
    if (vs != PERSIST_VERSION)
    {
        log_error("Persistent data version unknown.");
        memset(&persist_data, 0, sizeof(persist_data));
        memset(&persist_measurements, 0, sizeof(persist_measurements));
        persist_data.version = PERSIST_VERSION;
        persist_data.log_debug_mask = DEBUG_SYS;
        persist_data.mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
        for (uint8_t i = 0; i < ADC_CC_COUNT; i++)
        {
            persist_data.cc_configs[i].ext_max_mA = 100000;
            persist_data.cc_configs[i].int_max_mV = 50;
        }
        return;
    }

    memcpy(&persist_data, persist_data_raw, sizeof(persist_data));
    memcpy(&persist_measurements, persist_measurements_raw, sizeof(persist_measurements));
    persist_data_valid = true;
}


void persist_commit()
{
    if (platform_persist_commit(&persist_data, &persist_measurements))
    {
        log_sys_debug("Flash successfully written.");
        persist_data_valid = true;
    }
    else log_error("Flash write failed");
}

void persist_set_fw_ready(uint32_t size)
{
    persist_data.pending_fw = size;
    persist_commit();
}

void persist_set_log_debug_mask(uint32_t mask)
{
    persist_data.log_debug_mask = mask;
    persist_commit();
}


uint32_t persist_get_log_debug_mask(void)
{
    if (!persist_data_valid)
        return DEBUG_SYS;

    return persist_data.log_debug_mask;
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



bool persist_set_cc_midpoints(uint32_t midpoints[ADC_CC_COUNT])
{
    if (!persist_data_valid)
    {
        return false;
    }
    memcpy(persist_data.cc_midpoints, midpoints, ADC_CC_COUNT * sizeof(persist_data.cc_midpoints[0]));
    persist_commit();
    return true;
}


bool persist_get_cc_midpoints(uint32_t midpoints[ADC_CC_COUNT])
{
    if (!persist_data_valid)
    {
        return false;
    }
    memcpy(midpoints, persist_data.cc_midpoints, ADC_CC_COUNT * sizeof(persist_data.cc_midpoints[0]));
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


void    persistent_wipe(void)
{
    platform_persist_wipe();
    platform_raw_msg("Factory Reset");
    platform_reset_sys();
}


cc_config_t * persist_get_cc_configs(void)
{
    return persist_data.cc_configs;
}


char* persist_get_serial_number(void)
{
    return persist_data.serial_number;
}
