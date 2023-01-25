#include <string.h>

#include "comms.h"

#include "lw.h"
#include "rak4270.h"
#include "rak3172.h"
#include "version.h"
#include "base_types.h"
#include "log.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "common.h"


uint16_t comms_get_mtu(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_get_mtu();
        case VERSION_ARCH_REV_C:
            return rak3172_get_mtu();
        default:
            return 0;
    }
}


bool comms_send_ready(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_ready();
        case VERSION_ARCH_REV_C:
            return rak3172_send_ready();
        default:
            return false;
    }
}


bool comms_send_str(char* str)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_str(str);
        case VERSION_ARCH_REV_C:
            return rak3172_send_ready();
        default:
            return false;
    }
}


bool comms_send_allowed(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_send_allowed();
        case VERSION_ARCH_REV_C:
            return rak3172_send_allowed();
        default:
            return false;
    }
}


void comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_send(hex_arr, arr_len);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_send(hex_arr, arr_len);
            return;
        default:
            return;
    }
}


void comms_init(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_init();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_init();
            return;
        default:
            return;
    }
}


void comms_reset(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_reset();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_reset();
            return;
        default:
            return;
    }
}


void comms_process(char* message)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_process(message);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_process(message);
            return;
        default:
            return;
    }
}


bool comms_get_connected(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return rak4270_get_connected();
        case VERSION_ARCH_REV_C:
            return rak3172_get_connected();
        default:
            return false;
    }
}


void comms_loop_iteration(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_loop_iteration();
            return;
        case VERSION_ARCH_REV_C:
            rak3172_loop_iteration();
            return;
        default:
            return;
    }
}


void comms_config_setup_str(char * str)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_config_setup_str(str);
            return;
        case VERSION_ARCH_REV_C:
            rak3172_config_setup_str(str);
            return;
        default:
            return;
    }
}


bool comms_get_id(char* str, uint8_t len)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            return lw_get_id(str, len);
        case VERSION_ARCH_REV_C:
            return lw_get_id(str, len);
        default:
            return false;
    }
}


void comms_send_cb(char * args)
{
    char * pos = skip_space(args);
    comms_send_str(pos);
}


void comms_config_cb(char * args)
{
    comms_config_setup_str(skip_space(args));
}


static void comms_conn_cb(char* args)
{
    if (comms_get_connected())
    {
        log_out("1 | Connected");
    }
    else
    {
        log_out("0 | Disconnected");
    }
}


static void comms_dbg_cb(char* args)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
}


struct cmd_link_t* comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "comms_send",   "Send comms message",       comms_send_cb                 , false , NULL },
                                       { "comms_config", "Set comms config",         comms_config_cb               , false , NULL },
                                       { "comms_conn",   "LoRa connected",           comms_conn_cb                 , false , NULL },
                                       { "comms_dbg",    "Comms Chip Debug",         comms_dbg_cb                  , false , NULL }};
    tail = add_commands(tail, cmds, ARRAY_SIZE(cmds));
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            tail = rak4270_add_commands(tail);
            break;
        case VERSION_ARCH_REV_C:
            tail = rak3172_add_commands(tail);
            break;
        default:
            break;
    }
    return tail;
}


void comms_power_down(void)
{
    switch(version_get_arch())
    {
        case VERSION_ARCH_REV_B:
            rak4270_power_down();
            break;
        case VERSION_ARCH_REV_C:
            rak3172_power_down();
            break;
        default:
            break;
    }
}
