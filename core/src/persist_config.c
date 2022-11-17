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

static bool                             persist_data_valid = false;
static persist_storage_t                persist_data __attribute__((aligned (16)));
static persist_measurements_storage_t   persist_measurements __attribute__((aligned (16)));

void persistent_init(void)
{
    persist_storage_t* persist_data_raw = platform_get_raw_persist();
    persist_measurements_storage_t* persist_measurements_raw = platform_get_measurements_raw_persist();

    if (!persist_data_raw || !persist_measurements_raw || persist_data_raw->version != PERSIST_VERSION)
    {
        log_error("Persistent data version unknown.");
        memset(&persist_data, 0, sizeof(persist_data));
        memset(&persist_measurements, 0, sizeof(persist_measurements));
        persist_data.version = PERSIST_VERSION;
        persist_data.log_debug_mask = DEBUG_SYS;
        persist_data.mins_interval = MEASUREMENTS_DEFAULT_TRANSMIT_INTERVAL;
        adcs_setup_default_mem(&persist_data.adc_persist_config, sizeof(adc_persist_config_t));
        persist_data_valid = true;
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
    persist_data.log_debug_mask = mask | DEBUG_SYS;
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


char* persist_get_serial_number(void)
{
    return persist_data.serial_number;
}


adc_persist_config_t* persist_get_adc_config(void)
{
    if (!persist_data_valid)
        return NULL;
    return &persist_data.adc_persist_config;
}


static void reset_cb(char *args)
{
    platform_reset_sys();
}


static void wipe_cb(char* args)
{
    persistent_wipe();
}


struct cmd_link_t* persist_config_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "save",         "Save config",              persist_commit                , false , NULL },
                                       { "reset",        "Reset device.",            reset_cb                      , false , NULL },
                                       { "wipe",         "Factory Reset",            wipe_cb                       , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
