#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include <osm/core/config.h>
#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/core/platform.h>
#include <osm/core/persist_config.h>
#include <osm/core/uart_rings.h>
#include "model.h"


bool persist_config_update(const persist_storage_t* from_config, persist_storage_t* to_config)
{
    uint16_t base_version = from_config->version >> 8;
    uint16_t model_version = from_config->version & 0xFF;
    if (PERSIST_BASE_VERSION == base_version)
    {
        memcpy(to_config, from_config, sizeof(persist_storage_t));
    }
    else
    {
        return false;
    }
    return model_config_update((const void*)&from_config->model_config, &to_config->model_config, model_version);
}


char * persist_get_serial_number(void)
{
    return persist_data.serial_number;
}


char*    persist_get_model(void)
{
    return persist_data.model_name;
}


char* persist_get_human_name(void)
{
    return persist_data.human_name;
}


void persist_set_log_debug_mask(uint32_t mask)
{
    persist_data.log_debug_mask = mask | DEBUG_SYS;
}


uint32_t persist_get_log_debug_mask(void)
{
    return persist_data.log_debug_mask;
}


void persistent_wipe(void)
{
    platform_persist_wipe();
    platform_raw_msg("Factory Reset");
    uart_rings_drain_all_out();
    platform_reset_sys();
}


static command_response_t _persist_commit_cb(char* args, cmd_ctx_t * ctx)
{
    persist_commit();
    return COMMAND_RESP_OK;
}


static command_response_t _reset_cb(char *args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"Resetting...");
    cmd_ctx_out(ctx,LOG_END_SPACER);
    uart_rings_drain_all_out();
    platform_reset_sys();
    return COMMAND_RESP_OK;
}


static command_response_t _wipe_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,LOG_END_SPACER);
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
