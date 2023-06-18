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

#define SERIAL_NUM_COMM_LEN         17


static struct cmd_link_t* _cmds;


static command_response_t _cmd_count_cb(char * args)
{
    log_out("IOs     : %u", ios_get_count());
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_version_cb(char * args)
{
    char model_name[MODEL_NAME_LEN+1];
    if (strlen(MODEL_NAME) > MODEL_NAME_LEN)
        memcpy(model_name, MODEL_NAME, MODEL_NAME_LEN);
    else
        snprintf(model_name, MODEL_NAME_LEN, MODEL_NAME);
    unsigned len = strnlen(model_name, MODEL_NAME_LEN);
    model_name[len] = 0;

    for (unsigned i = 0; i < len; i++)
        model_name[i] = toupper(model_name[i]);

    log_out("Version : %s-%s", model_name, GIT_VERSION);
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_debug_cb(char * args)
{
    char * pos = skip_space(args);

    log_out("Debug mask : 0x%"PRIx32, log_debug_mask);

    if (pos[0])
    {
        if (pos[0] == '0' && (pos[1] == 'x' || pos[1] == 'X'))
            pos += 2;

        unsigned mask = strtoul(pos, &pos, 16);

        mask |= DEBUG_SYS;

        log_debug_mask = mask;
        persist_set_log_debug_mask(mask);
        log_out("Setting debug mask to 0x%x", mask);
    }
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_timer_cb(char* args)
{
    char* pos = skip_space(args);
    uint32_t delay_ms = strtoul(pos, NULL, 10);
    uint32_t start_time = get_since_boot_ms();
    timer_delay_us_64(delay_ms * 1000);
    log_out("Time elapsed: %"PRIu32, since_boot_delta(get_since_boot_ms(), start_time));
    return COMMAND_RESP_OK;
}


static command_response_t _cmd_serial_num_cb(char* args)
{
    char* serial_num = persist_get_serial_number();
    char* p = skip_space(args);
    uint8_t len = strnlen(p, SERIAL_NUM_LEN);
    if (len == 0)
        goto print_exit;
    log_out("Updating serial number.");
    strncpy(serial_num, p, len);
    serial_num[len] = 0;
print_exit:
    log_out("Serial Number: %s", serial_num);
    return COMMAND_RESP_OK;
}


command_response_t cmds_process(char * command, unsigned len)
{
    if (!_cmds)
    {
        log_out("Commands not filled.");
        return COMMAND_RESP_ERR;
    }

    if (!len)
        return COMMAND_RESP_ERR;

    log_sys_debug("Command \"%s\"", command);

    bool found = false;
    log_out(LOG_START_SPACER);
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
            resp = cmd->cb(args);
            break;
        }
    }
    if (!found)
    {
        log_out("Unknown command \"%s\"", command);
        log_out(LOG_SPACER);
        for(struct cmd_link_t * cmd = _cmds; cmd; cmd = cmd->next)
        {
            if (!cmd->hidden)
                log_out("%10s : %s", cmd->key, cmd->desc);
        }
    }
    log_out(LOG_END_SPACER);
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
    };

    struct cmd_link_t* tail = &cmds[ARRAY_SIZE(cmds)-1];

    for (struct cmd_link_t* cur = cmds; cur != tail; cur++)
        cur->next = cur + 1;

    model_cmds_add_all(tail);
    _cmds = cmds;
}
