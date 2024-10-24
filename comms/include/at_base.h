#pragma once

#include <string.h>

#include "base_types.h"


#define AT_BASE_MAX_CMD_LEN                                  (1024 + 128)

#define AT_BASE_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"AT "COMMS_ID_STR"\",\n\r  \"config\": {"
#define AT_BASE_PRINT_CFG_JSON_TAIL                         "  }\n\r}"

#define AT_BASE_MAC_ADDRESS_LEN                             18

#define AT_BASE_PRINT_CFG_JSON_FMT_MAC_ADDRESS              "    \"HWID\": \"%.*s\""
#define AT_BASE_PRINT_CFG_JSON_MAC_ADDRESS(_mac)            AT_BASE_PRINT_CFG_JSON_FMT_MAC_ADDRESS  , AT_BASE_MAC_ADDRESS_LEN       , _mac

#define AT_BASE_SANIT_STR(_s, _l)                                       \
    comms_common_json_escape(_s, _l, '\\', "\",", 2)


typedef struct
{
    char        str[AT_BASE_MAX_CMD_LEN];
    unsigned    len;
} at_base_cmd_t;

typedef struct
{
    uint64_t ts_unix;
    uint32_t sys;
} at_base_time_t;

typedef struct
{
    uint32_t        last_sent;
    uint32_t        last_recv;
    uint32_t        off_since;
    at_base_cmd_t    last_cmd;
    port_n_pins_t   reset_pin;
    port_n_pins_t   boot_pin;
    at_base_time_t   time;
    at_base_cmd_t    cmd_line;
    char            mac_address[AT_BASE_MAC_ADDRESS_LEN];
} at_base_ctx_t;


unsigned    at_base_raw_send(char* msg, unsigned len);
bool        at_base_send_str(char* str);
void        at_base_init(at_base_ctx_t* ctx);
bool        at_base_is_ok(char* msg, unsigned len);
bool        at_base_is_error(char* msg, unsigned len);
void        at_base_sleep(void);
void        at_base_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx);
bool        at_base_config_get_set_u16(const char* name, uint16_t* dest, char* src, cmd_ctx_t * ctx);
void        at_base_boot(char* args, cmd_ctx_t * ctx);
void        at_base_reset(char* args, cmd_ctx_t * ctx);
command_response_t at_base_config_setup_str(struct cmd_link_t * cmds, char * str, cmd_ctx_t * ctx);
