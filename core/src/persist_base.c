#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "config.h"
#include "log.h"
#include "common.h"
#include "platform.h"
#include "persist_config.h"


void persistent_wipe(void)
{
    platform_persist_wipe();
    platform_raw_msg("Factory Reset");
    platform_reset_sys();
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


static measurements_sensor_state_t _persist_config_measurements_get(char* name, measurements_reading_t* value)
{
    if (!value)
    {
        log_error("Handed NULL pointer.");
        return MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_i64 = (int64_t)persist_data.config_count;
    return MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static measurements_value_type_t _persist_config_value_type(char* name)
{
    return MEASUREMENTS_VALUE_TYPE_I64;
}


void persist_config_inf_init(measurements_inf_t* inf)
{
    inf->get_cb             = _persist_config_measurements_get;
    inf->value_type_cb      = _persist_config_value_type;
}
