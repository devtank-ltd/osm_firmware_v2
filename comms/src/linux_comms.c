#include <inttypes.h>
#include <string.h>
#include <stdio.h>


#include "config.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "log.h"
#include "common.h"


#define COMMS_DEFAULT_MTU       256
#define COMMS_ID_STR            "LINUX_COMMS"


uint16_t linux_comms_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool linux_comms_send_ready(void)
{
    return true;
}


bool linux_comms_send_str(char* str)
{
    if(!uart_ring_out(COMMS_UART, str, strlen(str)))
        return false;
    return uart_ring_out(COMMS_UART, "\r\n", 2);
}


bool linux_comms_send_allowed(void)
{
    return true;
}


void linux_comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIx32, hex_arr[i]);
        uart_ring_out(COMMS_UART, buf, 2);
    }
    uart_ring_out(COMMS_UART, "\r\n", 2);
    on_comms_sent_ack(true);
}


void linux_comms_init(void)
{
}


void linux_comms_reset(void)
{
}


void linux_comms_process(char* message)
{
}


bool linux_comms_get_connected(void)
{
    return true;
}


void linux_comms_loop_iteration(void)
{
}


void linux_comms_config_setup_str(char * str)
{
    if (strstr(str, "dev-eui"))
    {
        log_out("Dev EUI: LINUX-DEV");
    }
    else if (strstr(str, "app-key"))
    {
        log_out("App Key: LINUX-APP");
    }
}


bool linux_comms_get_id(char* str, uint8_t len)
{
    strncpy(str, COMMS_ID_STR, len);
    return true;
}


static command_response_t _linux_comms_send_cb(char * args)
{
    char * pos = skip_space(args);
    return linux_comms_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


static command_response_t _linux_comms_config_cb(char * args)
{
    linux_comms_config_setup_str(skip_space(args));
    return COMMAND_RESP_OK;
}


static command_response_t _linux_comms_conn_cb(char* args)
{
    if (linux_comms_get_connected())
    {
        log_out("1 | Connected");
    }
    else
    {
        log_out("0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


static command_response_t _linux_comms_dbg_cb(char* args)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* linux_comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] = {{ "comms_send",   "Send linux_comms message",        _linux_comms_send_cb        , false , NULL },
                                       { "comms_config", "Set linux_comms config",          _linux_comms_config_cb      , false , NULL },
                                       { "comms_conn",   "LoRa connected",                  _linux_comms_conn_cb        , false , NULL },
                                       { "comms_dbg",    "Comms Chip Debug",                _linux_comms_dbg_cb         , false , NULL }};
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
