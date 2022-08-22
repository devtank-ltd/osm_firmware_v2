#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "timers.h"
#include "lorawan.h"
#include "persist_config_header.h"
#include "flash_data.h"
#include "platform.h"

static bool                 persist_data_valid = false;
static bool                 persist_data_lw_valid = false;
static persist_storage_t    persist_data __attribute__((aligned (16)));


static void _lw_config_valid(void)
{
    persist_storage_t* persist_data_raw = platform_get_raw_persist();

    if (persist_data_raw->lw_dev_eui[0] && persist_data_raw->lw_app_key[0])
    {
        persist_data_lw_valid = true;
        for(unsigned n = 0; n < LW_DEV_EUI_LEN; n++)
        {
            if (!isascii(persist_data_raw->lw_dev_eui[n]))
            {
                persist_data_lw_valid = false;
                log_error("Persistent data lw dev not valid");
                break;
            }
        }
        for(unsigned n = 0; n < LW_APP_KEY_LEN; n++)
        {
            if (!isascii(persist_data_raw->lw_app_key[n]))
            {
                persist_data_lw_valid = false;
                log_error("Persistent data lw app not valid");
                break;
            }
        }
    }
}


void persistent_init(void)
{
    persist_storage_t* persist_data_raw = (persist_storage_t*)PERSIST_RAW_DATA;

    uint32_t vs = persist_data_raw->version;
    if (vs != PERSIST_VERSION)
    {
        log_error("Persistent data version unknown.");
        memset(&persist_data, 0, sizeof(persist_data));
        persist_data.version = PERSIST_VERSION;
        persist_data.log_debug_mask = DEBUG_SYS;
        persist_data.mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
        return;
    }

    _lw_config_valid();

    memcpy(&persist_data, persist_data_raw, sizeof(persist_data));
    persist_data_valid = true;
}


void persist_commit()
{
    platform_persist_commit();

    if (memcmp(PERSIST_RAW_DATA, &persist_data, sizeof(persist_data)) == 0)
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


void persist_set_lw_dev_eui(char* dev_eui)
{
    persist_data.version = PERSIST_VERSION;
    unsigned len = (dev_eui)?strlen(dev_eui):(LW_DEV_EUI_LEN+1);
    if (len > LW_DEV_EUI_LEN)
    {
        log_error("LORAWAN DEV EUI invalid.");
        return;
    }
    memcpy(persist_data.lw_dev_eui, dev_eui, len);
    persist_commit();
    log_debug(DEBUG_LW, "lw dev set");
    _lw_config_valid();
}


bool persist_get_lw_dev_eui(char dev_eui[LW_DEV_EUI_LEN + 1])
{
    if (!persist_data_lw_valid || !dev_eui)
    {
        log_error("LORAWAN DEV EUI get failed.");
        return false;
    }
    memcpy(dev_eui, persist_data.lw_dev_eui, LW_DEV_EUI_LEN);
    dev_eui[LW_DEV_EUI_LEN] = 0;
    return true;
}


void persist_set_lw_app_key(char* app_key)
{
    persist_data.version = PERSIST_VERSION;
    unsigned len = (app_key)?strlen(app_key):(LW_APP_KEY_LEN+1);
    if (len > LW_APP_KEY_LEN)
    {
        log_error("LORAWAN APP KEY invalid.");
        return;
    }
    memcpy(persist_data.lw_app_key, app_key, len);
    persist_commit();
    log_debug(DEBUG_LW, "lw app set");
    _lw_config_valid();
}


bool persist_get_lw_app_key(char app_key[LW_APP_KEY_LEN+1])
{
    if (!persist_data_lw_valid || !app_key)
    {
        log_error("LORAWAN APP KEY get failed.");
        return false;
    }
    memcpy(app_key, persist_data.lw_app_key, LW_APP_KEY_LEN);
    app_key[LW_APP_KEY_LEN] = 0;
    return true;
}


measurements_def_t * persist_get_measurements(void)
{
    return persist_data.measurements_arr;
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



bool persist_set_cc_midpoints(uint16_t midpoints[ADC_CC_COUNT])
{
    if (!persist_data_valid)
    {
        return false;
    }
    memcpy(persist_data.cc_midpoints, midpoints, ADC_CC_COUNT * sizeof(persist_data.cc_midpoints[0]));
    persist_commit();
    return true;
}


bool persist_get_cc_midpoints(uint16_t midpoints[ADC_CC_COUNT])
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
