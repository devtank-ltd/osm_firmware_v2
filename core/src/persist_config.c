#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "platform.h"
#include "platform_model.h"
#include "common.h"
#include "io.h"


persist_storage_t               persist_data __attribute__((aligned (16)));
persist_measurements_storage_t  persist_measurements __attribute__((aligned (16)));


bool persistent_init(void)
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
        if (strlen(MODEL_NAME) > MODEL_NAME_LEN)
            memcpy(&persist_data.model_name[0], MODEL_NAME, MODEL_NAME_LEN);
        else
            snprintf(persist_data.model_name, MODEL_NAME_LEN, MODEL_NAME);
        model_persist_config_model_init(&persist_data.model_config);
        return false;
    }

    memcpy(&persist_data, persist_data_raw, sizeof(persist_data));
    memcpy(&persist_measurements, persist_measurements_raw, sizeof(persist_measurements));
    return true;
}


void persist_commit()
{
    if (platform_persist_commit(&persist_data, &persist_measurements))
        log_sys_debug("Flash successfully written.");
    else
        log_error("Flash write failed");
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
    return persist_data.log_debug_mask;
}


void persistent_wipe(void)
{
    platform_persist_wipe();
    platform_raw_msg("Factory Reset");
    platform_reset_sys();
}


char* persist_get_serial_number(void)
{
    return persist_data.serial_number;
}


static command_response_t _persist_commit_cb(char* args)
{
    persist_commit();
    return COMMAND_RESP_OK;
}


static command_response_t _reset_cb(char *args)
{
    platform_reset_sys();
    return COMMAND_RESP_OK;
}


static command_response_t _wipe_cb(char* args)
{
    persistent_wipe();
    return COMMAND_RESP_OK;
}


struct cmd_link_t* persist_config_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "save",         "Save config",             _persist_commit_cb             , false , NULL },
                                       { "reset",        "Reset device.",           _reset_cb                      , false , NULL },
                                       { "wipe",         "Factory Reset",           _wipe_cb                       , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
