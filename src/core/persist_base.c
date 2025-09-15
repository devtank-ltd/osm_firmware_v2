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


bool osm_persist_config_update(const osm_persist_storage_t* from_config, osm_persist_storage_t* to_config)
{
    uint16_t base_version = from_config->version >> 8;
    uint16_t model_version = from_config->version & 0xFF;
    if (OSM_PERSIST_BASE_VERSION == base_version)
    {
        memcpy(to_config, from_config, sizeof(osm_persist_storage_t));
    }
    else
    {
        return false;
    }
    return osm_model_config_update((const void*)&from_config->model_config, &to_config->model_config, model_version);
}


char * osm_persist_get_serial_number(void)
{
    return persist_data.serial_number;
}


char*    osm_persist_get_model(void)
{
    return persist_data.model_name;
}


char* osm_persist_get_human_name(void)
{
    return persist_data.human_name;
}


void osm_persist_set_log_debug_mask(uint32_t mask)
{
    persist_data.log_debug_mask = mask | OSM_DEBUG_SYS;
}


uint32_t osm_persist_get_log_debug_mask(void)
{
    return persist_data.log_debug_mask;
}


void osm_persistent_wipe(void)
{
    osm_platform_persist_wipe();
    osm_platform_raw_msg("Factory Reset");
    osm_uart_rings_drain_all_out();
    osm_platform_reset_sys();
}


static osm_command_response_t _persist_commit_cb(char* args, osm_cmd_ctx_t * ctx)
{
    osm_persist_commit();
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _reset_cb(char *args, osm_cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,"Resetting...");
    osm_cmd_ctx_out(ctx,OSM_LOG_END_SPACER);
    osm_uart_rings_drain_all_out();
    osm_platform_reset_sys();
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _wipe_cb(char* args, osm_cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,OSM_LOG_END_SPACER);
    osm_persistent_wipe();
    return OSM_COMMAND_RESP_OK;
}


struct osm_cmd_link_t* osm_persist_config_add_commands(struct osm_cmd_link_t* tail)
{
    static struct osm_cmd_link_t cmds[] = {{ "save",         "Save config",             _persist_commit_cb             , false , NULL },
                                       { "reset",        "Reset device.",           _reset_cb                      , false , NULL },
                                       { "wipe",         "Factory Reset",           _wipe_cb                       , false , NULL }};
    return osm_add_commands(tail, cmds, OSM_ARRAY_SIZE(cmds));
}


static osm_measurements_sensor_state_t _persist_config_measurements_get(char* name, osm_measurements_reading_t* value)
{
    if (!value)
    {
        osm_log_error("Handed NULL pointer.");
        return OSM_MEASUREMENTS_SENSOR_STATE_ERROR;
    }
    value->v_i64 = (int64_t)persist_data.config_count;
    return OSM_MEASUREMENTS_SENSOR_STATE_SUCCESS;
}


static osm_measurements_value_type_t _persist_config_value_type(char* name)
{
    return OSM_MEASUREMENTS_VALUE_TYPE_I64;
}


void osm_persist_config_inf_init(osm_measurements_inf_t* inf)
{
    inf->get_cb             = _persist_config_measurements_get;
    inf->value_type_cb      = _persist_config_value_type;
}
