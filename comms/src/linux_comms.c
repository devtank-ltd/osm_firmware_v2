#include <inttypes.h>
#include <string.h>
#include <stdio.h>


#include "config.h"
#include "uart_rings.h"
#include "pinmap.h"
#include "log.h"
#include "common.h"
#include "protocol.h"
#include "cmd.h"


#define COMMS_DEFAULT_MTU       256
#define COMMS_ID_STR            "LINUX_COMMS"

#define LINUX_COMMS_DEV_EUI     "LINUX-DEV"
#define LINUX_COMMS_APP_KEY     "LINUX-APP"

#define LINUX_COMMS_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"LW PENGUIN\",\n\r  \"config\": {"
#define LINUX_COMMS_PRINT_CFG_JSON_DEV_EUI                      "    \"DEV EUI\": \""LINUX_COMMS_DEV_EUI"\","
#define LINUX_COMMS_PRINT_CFG_JSON_APP_KEY                      "    \"APP KEY\": \""LINUX_COMMS_APP_KEY"\""
#define LINUX_COMMS_PRINT_CFG_JSON_TAIL                         "  }\n\r}"


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


bool linux_comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIx32, hex_arr[i]);
        uart_ring_out(COMMS_UART, buf, 2);
    }
    uart_ring_out(COMMS_UART, "\r\n", 2);
    while (uart_ring_out_busy(COMMS_UART))
    {
        uart_rings_out_drain();
    }
    on_protocol_sent_ack(true);
    return true;
}


void linux_comms_init(void)
{
}


void linux_comms_reset(void)
{
}


void linux_comms_process(char* message)
{
    unsigned msg_len = strnlen(message, CMD_LINELEN);
    cmds_process(message, msg_len, NULL);
}


bool linux_comms_get_connected(void)
{
    return true;
}


void linux_comms_loop_iteration(void)
{
}


void linux_comms_config_setup_str(char * str, cmd_ctx_t * ctx)
{
    if (strstr(str, "dev-eui"))
    {
        cmd_ctx_out(ctx,"Dev EUI: "LINUX_COMMS_DEV_EUI);
    }
    else if (strstr(str, "app-key"))
    {
        cmd_ctx_out(ctx,"App Key: LINUX-APP");
    }
}


bool linux_comms_get_id(char* str, uint8_t len)
{
    strncpy(str, COMMS_ID_STR, len);
    return true;
}


static command_response_t _linux_comms_send_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = skip_space(args);
    return linux_comms_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t linux_comms_cmd_config_cb(char * args, cmd_ctx_t * ctx)
{
    linux_comms_config_setup_str(skip_space(args), ctx);
    return COMMAND_RESP_OK;
}


command_response_t linux_comms_cmd_conn_cb(char* args, cmd_ctx_t * ctx)
{
    if (linux_comms_get_connected())
    {
        cmd_ctx_out(ctx,"1 | Connected");
    }
    else
    {
        cmd_ctx_out(ctx,"0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


command_response_t linux_comms_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx)
{
    cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_HEADER);
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_DEV_EUI);
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_APP_KEY);
    cmd_ctx_flush(ctx);
    cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_TAIL);
    cmd_ctx_flush(ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _linux_comms_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    uart_ring_out(COMMS_UART, args, strlen(args));
    uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* linux_comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "comms_send"  ,  "Send linux_comms message"   , _linux_comms_send_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"            , _linux_comms_dbg_cb           , false , NULL }
    };
    return add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
