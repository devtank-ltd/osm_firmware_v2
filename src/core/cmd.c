#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include <osm/core/cmd.h>
#include <osm/core/io.h>
#include <osm/core/timers.h>
#include <osm/core/persist_config.h>
#include <osm/core/common.h>
#include <osm/core/log.h>
#include <osm/core/platform.h>
#include "platform_model.h"
#include <osm/core/uart_rings.h>

#define SERIAL_NUM_COMM_LEN         17


static struct cmd_link_t* _cmds;


static osm_command_response_t _cmd_count_cb(char * args, cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,"IOs     : %u", osm_ios_get_count());
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _cmd_version_cb(char * args, cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,"Version : %s-%s", osm_persist_get_model(), GIT_VERSION);
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _cmd_debug_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = osm_skip_space(args);

    osm_cmd_ctx_out(ctx,"Debug mask : 0x%"PRIx32, log_debug_mask);

    if (pos[0])
    {
        if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
            pos += 2;

        unsigned mask = strtoul(pos, &pos, 16);

        mask |= OSM_DEBUG_SYS;

        log_debug_mask = mask;
        osm_persist_set_log_debug_mask(mask);
        osm_cmd_ctx_out(ctx,"Setting debug mask to 0x%x", mask);
    }
    return OSM_COMMAND_RESP_OK;
}


static osm_command_response_t _cmd_timer_cb(char* args, cmd_ctx_t * ctx)
{
    char* pos = osm_skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = osm_get_since_boot_ms();
    osm_timer_delay_us_64(delay_ms * 1000);
    osm_cmd_ctx_out(ctx,"Time elapsed: %"PRIu32, osm_since_boot_delta(osm_get_since_boot_ms(), start_time));
    return OSM_COMMAND_RESP_OK;
}


static bool _cmd_get_set_str(const char* name, char* mem, unsigned mem_len, char* arg, cmd_ctx_t * ctx)
{
    bool ret = false;
    if (name && mem && arg)
    {
        uint8_t len = strnlen(arg, mem_len);
        if (len == 0)
            goto print_exit;
        osm_cmd_ctx_out(ctx,"Updating %s.", name);
        strncpy(mem, arg, len);
        mem[len] = 0;
print_exit:
        osm_cmd_ctx_out(ctx,"%s: %s", name, mem);
        ret = true;
    }
    return ret;
}


static osm_command_response_t _cmd_serial_num_cb(char* args, cmd_ctx_t * ctx)
{
    return _cmd_get_set_str("Serial Number", osm_persist_get_serial_number(), OSM_SERIAL_NUM_LEN, osm_skip_space(args), ctx) ?
        OSM_COMMAND_RESP_OK : OSM_COMMAND_RESP_ERR;
}


static osm_command_response_t _cmd_human_name_cb(char* args, cmd_ctx_t * ctx)
{
    return _cmd_get_set_str("Name", osm_persist_get_human_name(), OSM_HUMAN_NAME_LEN, osm_skip_space(args), ctx) ?
        OSM_COMMAND_RESP_OK : OSM_COMMAND_RESP_ERR;
}


static osm_command_response_t _cmd_hw_id_cb(char* args, cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,"hw_id: 0x%"PRIX32, osm_platform_get_hw_id());
    return OSM_COMMAND_RESP_OK;
}


osm_command_response_t osm_cmds_process(char * command, unsigned len, cmd_ctx_t * ctx)
{
    if (!_cmds)
    {
        osm_cmd_ctx_out(ctx,"Commands not filled.");
        return OSM_COMMAND_RESP_ERR;
    }

    if (!len)
        return OSM_COMMAND_RESP_ERR;

    osm_log_sys_debug("Command \"%s\"", command);

    bool found = false;
    osm_cmd_ctx_out(ctx,OSM_LOG_START_SPACER);
    osm_command_response_t resp = OSM_COMMAND_RESP_ERR;
    char * args;
    for(struct cmd_link_t * cmd = _cmds; cmd; cmd = cmd->next)
    {
        unsigned keylen = strlen(cmd->key);
        if(len >= keylen &&
           !strncmp(cmd->key, command, keylen) &&
           (command[keylen] == '\0' || command[keylen] == ' '))
        {
            found = true;
            args = osm_skip_space(command + keylen);
            while(command[len-1] == ' ')
                command[--len] = 0;
            resp = cmd->cb(args, ctx);
            break;
        }
    }
    if (!found)
    {
        osm_cmd_ctx_out(ctx,"Unknown command \"%s\"", command);
        osm_cmd_ctx_out(ctx,OSM_LOG_SPACER);
        for(struct cmd_link_t * cmd = _cmds; cmd; cmd = cmd->next)
        {
            if (!cmd->hidden)
            {
                osm_cmd_ctx_out(ctx,"%10s : %s", cmd->key, cmd->desc);
                osm_uart_rings_drain_all_out();
            }
        }
    }
    osm_cmd_ctx_out(ctx,OSM_LOG_END_SPACER);
    osm_log_sys_debug("Command Response Code:%d", (int)resp);
    return resp;
}


void osm_cmds_init(void)
{
    static struct cmd_link_t cmds[] = {
        { "count",        "Counts of controls.",      _cmd_count_cb                  , false , NULL},
        { "version",      "Print version.",           _cmd_version_cb                , false , NULL},
        { "debug",        "Set hex debug mask",       _cmd_debug_cb                  , false , NULL},
        { "timer",        "Test usecs timer",         _cmd_timer_cb                  , false , NULL},
        { "serial_num",   "Set/get serial number",    _cmd_serial_num_cb             , true  , NULL},
        { "name",         "Set/get serial number",    _cmd_human_name_cb             , false , NULL},
        { "hw_id",        "Get Hardware ID",          _cmd_hw_id_cb                  , false , NULL},
    };

    struct cmd_link_t* tail = &cmds[OSM_ARRAY_SIZE(cmds)-1];

    for (struct cmd_link_t* cur = cmds; cur != tail; cur++)
        cur->next = cur + 1;

    osm_model_cmds_add_all(tail);
    _cmds = cmds;
}
