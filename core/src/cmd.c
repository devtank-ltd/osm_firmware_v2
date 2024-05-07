#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "cmd.h"
#include "io.h"
#include "timers.h"
#include "persist_config.h"
#include "common.h"
#include "log.h"
#include "platform_model.h"
#include "uart_rings.h"

#define SERIAL_NUM_COMM_LEN         17


static struct cmd_link_t* _cmds;


static command_response_t _cmd_count_cb(char * args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"IOs     : %u", ios_get_count());
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_version_cb(char * args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,"Version : %s-%s", persist_get_model(), GIT_VERSION);
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_debug_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = skip_space(args);

    cmd_ctx_out(ctx,"Debug mask : 0x%"PRIx32, log_debug_mask);

    if (pos[0])
    {
        if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
            pos += 2;

        unsigned mask = strtoul(pos, &pos, 16);

        mask |= DEBUG_SYS;

        log_debug_mask = mask;
        persist_set_log_debug_mask(mask);
        cmd_ctx_out(ctx,"Setting debug mask to 0x%x", mask);
    }
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_timer_cb(char* args, cmd_ctx_t * ctx)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    cmd_ctx_out(ctx,"Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
    return COMMAND_RESP_OK;
}


static bool _cmd_get_set_str(const char* name, char* mem, unsigned mem_len, char* arg, cmd_ctx_t * ctx)
{
    bool ret = false;
    if (name && mem && arg)
    {
        uint8_t len = strnlen(arg, mem_len);
        if (len == 0)
            goto print_exit;
        cmd_ctx_out(ctx,"Updating %s.", name);
        strncpy(mem, arg, len);
        mem[len] = 0;
print_exit:
        cmd_ctx_out(ctx,"%s: %s", name, mem);
        ret = true;
    }
    return ret;
}


static command_response_t _cmd_serial_num_cb(char* args, cmd_ctx_t * ctx)
{
    return _cmd_get_set_str("Serial Number", persist_get_serial_number(), SERIAL_NUM_LEN, skip_space(args), ctx) ?
        COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _cmd_human_name_cb(char* args, cmd_ctx_t * ctx)
{
    return _cmd_get_set_str("Name", persist_get_human_name(), HUMAN_NAME_LEN, skip_space(args), ctx) ?
        COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t cmds_process(char * command, unsigned len, cmd_ctx_t * ctx)
{
    if (!_cmds)
    {
        cmd_ctx_out(ctx,"Commands not filled.");
        return COMMAND_RESP_ERR;
    }

    if (!len)
        return COMMAND_RESP_ERR;

    log_sys_debug("Command \"%s\"", command);

    bool found = false;
    cmd_ctx_out(ctx,LOG_START_SPACER);
    command_response_t resp = COMMAND_RESP_ERR;
    char * args;
    for(struct cmd_link_t * cmd = _cmds; cmd; cmd = cmd->next)
    {
        unsigned keylen = strlen(cmd->key);
        if(len >= keylen &&
           !strncmp(cmd->key, command, keylen) &&
           (command[keylen] == '\0' || command[keylen] == ' '))
        {
            found = true;
            args = skip_space(command + keylen);
            while(command[len-1] == ' ')
                command[--len] = 0;
            resp = cmd->cb(args, ctx);
            break;
        }
    }
    if (!found)
    {
        cmd_ctx_out(ctx,"Unknown command \"%s\"", command);
        cmd_ctx_out(ctx,LOG_SPACER);
        for(struct cmd_link_t * cmd = _cmds; cmd; cmd = cmd->next)
        {
            if (!cmd->hidden)
            {
                cmd_ctx_out(ctx,"%10s : %s", cmd->key, cmd->desc);
                uart_rings_drain_all_out();
            }
        }
    }
    cmd_ctx_out(ctx,LOG_END_SPACER);
    return resp;
}


void cmds_init(void)
{
    static struct cmd_link_t cmds[] = {
        { "count",        "Counts of controls.",      _cmd_count_cb                  , false , NULL},
        { "version",      "Print version.",           _cmd_version_cb                , false , NULL},
        { "debug",        "Set hex debug mask",       _cmd_debug_cb                  , false , NULL},
        { "timer",        "Test usecs timer",         _cmd_timer_cb                  , false , NULL},
        { "serial_num",   "Set/get serial number",    _cmd_serial_num_cb             , true  , NULL},
        { "name",         "Set/get serial number",    _cmd_human_name_cb             , false , NULL},
    };

    struct cmd_link_t* tail = &cmds[ARRAY_SIZE(cmds)-1];

    for (struct cmd_link_t* cur = cmds; cur != tail; cur++)
        cur->next = cur + 1;

    model_cmds_add_all(tail);
    _cmds = cmds;
}
