#include <inttypes.h>
#include <string.h>
#include <stdio.h>


#include <osm/core/config.h>
#include <osm/core/uart_rings.h>
#include "pinmap.h"
#include <osm/core/log.h>
#include <osm/core/common.h>
#include <osm/protocols/protocol.h>
#include <osm/core/cmd.h>


#define COMMS_DEFAULT_MTU       256
#define COMMS_ID_STR            "LINUX_COMMS"

#define LINUX_COMMS_DEV_EUI     "LINUX-DEV"
#define LINUX_COMMS_APP_KEY     "LINUX-APP"

#define LINUX_COMMS_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"LW\",\n\r  \"config\": {"
#define LINUX_COMMS_PRINT_CFG_JSON_DEV_EUI                      "    \"DEV EUI\": \""LINUX_COMMS_DEV_EUI"\","
#define LINUX_COMMS_PRINT_CFG_JSON_APP_KEY                      "    \"APP KEY\": \""LINUX_COMMS_APP_KEY"\""
#define LINUX_COMMS_PRINT_CFG_JSON_TAIL                         "  }\n\r}"


uint16_t osm_linux_comms_get_mtu(void)
{
    return COMMS_DEFAULT_MTU;
}


bool osm_linux_comms_send_ready(void)
{
    return true;
}


bool osm_linux_comms_send_str(char* str)
{
    if(!osm_uart_ring_out(COMMS_UART, str, strlen(str)))
        return false;
    return osm_uart_ring_out(COMMS_UART, "\r\n", 2);
}


bool osm_linux_comms_send_allowed(void)
{
    return true;
}


bool osm_linux_comms_send(int8_t* hex_arr, uint16_t arr_len)
{
    char buf[3];
    for (uint16_t i = 0; i < arr_len; i++)
    {
        snprintf(buf, 3, "%.2"PRIx32, hex_arr[i]);
        osm_uart_ring_out(COMMS_UART, buf, 2);
    }
    osm_uart_ring_out(COMMS_UART, "\r\n", 2);
    while (osm_uart_ring_out_busy(COMMS_UART))
    {
        osm_uart_rings_out_drain();
    }
    osm_on_protocol_sent_ack(true);
    return true;
}



void     osm_linux_comms_config_init(comms_config_t* config)
{
    config->type = COMMS_BUILD_TYPE;
}


bool     osm_linux_comms_persist_config_cmp(comms_config_t* d0, comms_config_t* d1)
{
    return (d0->type == d1->type);
}


void osm_linux_comms_init(void)
{
}


void osm_linux_comms_reset(void)
{
}


void osm_linux_comms_process(char* message)
{
    unsigned msg_len = strnlen(message, CMD_LINELEN);
    osm_cmds_process(message, msg_len, NULL);
}


bool osm_linux_comms_get_connected(void)
{
    return true;
}


void osm_linux_comms_loop_iteration(void)
{
}


void osm_linux_comms_config_setup_str(char * str, cmd_ctx_t * ctx)
{
    if (strstr(str, "dev-eui"))
    {
        osm_cmd_ctx_out(ctx,"Dev EUI: "LINUX_COMMS_DEV_EUI);
    }
    else if (strstr(str, "app-key"))
    {
        osm_cmd_ctx_out(ctx,"App Key: LINUX-APP");
    }
}


bool osm_linux_comms_get_id(char* str, uint8_t len)
{
    strncpy(str, COMMS_ID_STR, len);
    return true;
}


static command_response_t _linux_comms_send_cb(char * args, cmd_ctx_t * ctx)
{
    char * pos = osm_skip_space(args);
    return osm_linux_comms_send_str(pos) ? COMMAND_RESP_OK : COMMAND_RESP_ERR;
}


command_response_t osm_linux_comms_cmd_config_cb(char * args, cmd_ctx_t * ctx)
{
    osm_linux_comms_config_setup_str(osm_skip_space(args), ctx);
    return COMMAND_RESP_OK;
}


command_response_t osm_linux_comms_cmd_conn_cb(char* args, cmd_ctx_t * ctx)
{
    if (osm_linux_comms_get_connected())
    {
        osm_cmd_ctx_out(ctx,"1 | Connected");
    }
    else
    {
        osm_cmd_ctx_out(ctx,"0 | Disconnected");
    }
    return COMMAND_RESP_OK;
}


command_response_t osm_linux_comms_cmd_j_cfg_cb(char* args, cmd_ctx_t * ctx)
{
    osm_cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_HEADER);
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_DEV_EUI);
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_APP_KEY);
    osm_cmd_ctx_flush(ctx);
    osm_cmd_ctx_out(ctx,LINUX_COMMS_PRINT_CFG_JSON_TAIL);
    osm_cmd_ctx_flush(ctx);
    return COMMAND_RESP_OK;
}


static command_response_t _linux_comms_dbg_cb(char* args, cmd_ctx_t * ctx)
{
    osm_uart_ring_out(COMMS_UART, args, strlen(args));
    osm_uart_ring_out(COMMS_UART, "\r\n", 2);
    return COMMAND_RESP_OK;
}


struct cmd_link_t* osm_linux_comms_add_commands(struct cmd_link_t* tail)
{
    static struct cmd_link_t cmds[] =
    {
        { "osm_comms_send"  ,  "Send linux_comms message"   , _linux_comms_send_cb          , false , NULL },
        { "comms_dbg"   , "Comms Chip Debug"            , _linux_comms_dbg_cb           , false , NULL }
    };
    return osm_add_commands(tail, cmds, ARRAY_SIZE(cmds));
}
