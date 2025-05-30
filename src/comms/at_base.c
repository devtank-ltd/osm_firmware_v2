#include <stdbool.h>
#include <stdlib.h>

#include "pinmap.h"
#include <osm/core/uart_rings.h>
#include <osm/core/platform.h>
#include <osm/core/common.h>
#include <osm/core/base_types.h>
#include <osm/core/log.h>


#define AT_BASE_MAC_ADDR_LEN            18


static at_base_ctx_t* _at_base_ctx = NULL;


unsigned at_base_raw_send(char* msg, unsigned len)
{
    return uart_ring_out(COMMS_UART, msg, len);
}


bool at_base_send_str(char* str)
{
    if(!at_base_raw_send(str, strlen(str)))
        return false;
    return at_base_raw_send("\r\n", 2);
}


void at_base_init(at_base_ctx_t* ctx)
{
    if (ctx)
    {
        _at_base_ctx = ctx;

        platform_gpio_init(&_at_base_ctx->reset_pin);
        platform_gpio_setup(&_at_base_ctx->reset_pin, false, IO_PUPD_UP);
        platform_gpio_set(&_at_base_ctx->reset_pin, true);
        platform_gpio_init(&_at_base_ctx->boot_pin);
        platform_gpio_setup(&_at_base_ctx->boot_pin, false, IO_PUPD_UP);
        platform_gpio_set(&_at_base_ctx->boot_pin, true);
    }
    else
    {
        comms_debug("Handed NULL pointer for context.");
    }
}


bool at_base_is_ok(char* msg, unsigned len)
{
    const char ok_msg[] = "OK";
    return is_str(ok_msg, msg, len);
}


bool at_base_is_error(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    return is_str(error_msg, msg, len);
}


static bool _at_base_sleep_loop_iterate(void *userdata)
{
    return false;
}


void at_base_sleep(void)
{
    main_loop_iterate_for(10, _at_base_sleep_loop_iterate, NULL);
}


void at_base_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx)
{
    unsigned len = strlen(src);
    if (len)
    {
        if (len > max_dest_len)
            len = max_dest_len;
        /* Set */
        memset(dest, 0, max_dest_len+1 /* Set full space, including final null*/);
        memcpy(dest, src, len);
    }
    /* Get */
    cmd_ctx_out(ctx,"%s: %.*s", name, max_dest_len, dest);
}


bool at_base_config_get_set_u16(const char* name, uint16_t* dest, char* src, cmd_ctx_t * ctx)
{
    bool ret = true;
    unsigned len = strlen(src);
    if (len && dest)
    {
        char* np, * p = src;
        uint16_t new_uint16 = strtol(p, &np, 10);
        if (p != np)
        {
            *dest = new_uint16;
        }
        else
        {
            ret = false;
        }
    }
    /* Get */
    cmd_ctx_out(ctx,"%s: %"PRIu16, name, *dest);
    return ret;
}


void at_base_boot(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_base_ctx->boot_pin, is_out);
    cmd_ctx_out(ctx,"BOOT PIN: %u", is_out ? 1U : 0U);
}


void at_base_reset(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_base_ctx->reset_pin, is_out);
    cmd_ctx_out(ctx,"RESET PIN: %u", is_out ? 1U : 0U);
}


command_response_t at_base_config_setup_str(struct cmd_link_t * cmds, char * str, cmd_ctx_t * ctx)
{
    command_response_t r = COMMAND_RESP_ERR;
    if (str[0])
    {
        char * next = skip_to_space(str);
        if (next[0])
        {
            char * t = next;
            next = skip_space(next);
            *t = 0;
        }
        if ('"' == next[0])
        {
            char* end_name = strchr(next+1, '"');
            if (end_name)
            {
                next++;
                while (end_name && '\\' == *(end_name - 1))
                {
                    unsigned len = strlen(end_name);
                    memmove(end_name - 1, end_name, len);
                    end_name[len - 1] = 0;
                    end_name = strchr(end_name, '"');
                }
                if (end_name)
                    *end_name = 0;
            }
        }
        for(struct cmd_link_t * cmd = cmds; cmd; cmd = cmd->next)
        {
            if (!strcmp(cmd->key, str) && cmd->cb)
                return cmd->cb(next, ctx);
        }
    }
    else r = COMMAND_RESP_OK;

    for(struct cmd_link_t * cmd = cmds; cmd; cmd = cmd->next)
    {
        if (!cmd->hidden)
            cmd_ctx_out(ctx,"%10s : %s", cmd->key, cmd->desc);
    }
    return r;
}
