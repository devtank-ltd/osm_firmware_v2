#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "log.h"
#include "persist_config.h"
#include "persist_config_header.h"
#include "platform.h"
#include "common.h"


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
        persist_config_model_init(&persist_data.model_config);
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
