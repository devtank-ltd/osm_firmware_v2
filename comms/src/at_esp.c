#include <stdbool.h>
#include <stdlib.h>

#include "pinmap.h"
#include "uart_rings.h"
#include "platform.h"
#include "common.h"
#include "base_types.h"
#include "log.h"


static at_esp_ctx_t* _at_esp_ctx = NULL;


unsigned at_esp_raw_send(char* msg, unsigned len)
{
    return uart_ring_out(COMMS_UART, msg, len);
}


bool at_esp_send_str(char* str)
{
    if(!at_esp_raw_send(str, strlen(str)))
        return false;
    return at_esp_raw_send("\r\n", 2);
}


void at_esp_init(at_esp_ctx_t* ctx)
{
    if (ctx)
    {
        _at_esp_ctx = ctx;

        platform_gpio_init(&_at_esp_ctx->reset_pin);
        platform_gpio_setup(&_at_esp_ctx->reset_pin, false, IO_PUPD_UP);
        platform_gpio_set(&_at_esp_ctx->reset_pin, true);
        platform_gpio_init(&_at_esp_ctx->boot_pin);
        platform_gpio_setup(&_at_esp_ctx->boot_pin, false, IO_PUPD_UP);
        platform_gpio_set(&_at_esp_ctx->boot_pin, true);
    }
    else
    {
        comms_debug("Handed NULL pointer for context.");
    }
}


bool at_esp_is_ok(char* msg, unsigned len)
{
    const char ok_msg[] = "OK";
    return is_str(ok_msg, msg, len);
}


bool at_esp_is_error(char* msg, unsigned len)
{
    const char error_msg[] = "ERROR";
    return is_str(error_msg, msg, len);
}


static bool _at_esp_sleep_loop_iterate(void *userdata)
{
    return false;
}


void at_esp_sleep(void)
{
    main_loop_iterate_for(10, _at_esp_sleep_loop_iterate, NULL);
}


void at_esp_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx)
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


bool at_esp_config_get_set_u16(const char* name, uint16_t* dest, char* src, cmd_ctx_t * ctx)
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


void at_esp_boot(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_esp_ctx->boot_pin, is_out);
    cmd_ctx_out(ctx,"BOOT PIN: %u", is_out ? 1U : 0U);
}


void at_esp_reset(char* args, cmd_ctx_t * ctx)
{
    bool is_out = (bool)strtoul(args, NULL, 10);
    platform_gpio_set(&_at_esp_ctx->reset_pin, is_out);
    cmd_ctx_out(ctx,"RESET PIN: %u", is_out ? 1U : 0U);
}
