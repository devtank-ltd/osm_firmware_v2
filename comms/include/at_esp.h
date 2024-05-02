#pragma once

#include <string.h>

#include "base_types.h"


#define AT_ESP_MAX_CMD_LEN                                  (1024 + 128)

#define AT_ESP_PRINT_CFG_JSON_HEADER                       "{\n\r  \"type\": \"AT "COMMS_ID_STR"\",\n\r  \"config\": {"
#define AT_ESP_PRINT_CFG_JSON_TAIL                         "  }\n\r}"


typedef struct
{
    char        str[AT_ESP_MAX_CMD_LEN];
    unsigned    len;
} at_esp_cmd_t;

typedef struct
{
    uint64_t ts_unix;
    uint32_t sys;
} at_esp_time_t;

typedef struct
{
    uint32_t        last_sent;
    uint32_t        last_recv;
    uint32_t        off_since;
    at_esp_cmd_t    last_cmd[AT_ESP_MAX_CMD_LEN];
    port_n_pins_t   reset_pin;
    port_n_pins_t   boot_pin;
    at_esp_time_t   time;
    at_esp_cmd_t    cmd_line[AT_ESP_MAX_CMD_LEN];
} at_esp_ctx_t;


unsigned    at_esp_raw_send(char* msg, unsigned len);
bool        at_esp_send_str(char* str);
void        at_esp_init(at_esp_ctx_t* ctx);
bool        at_esp_is_ok(char* msg, unsigned len);
bool        at_esp_is_error(char* msg, unsigned len);
void        at_esp_sleep(void);
void        at_esp_config_get_set_str(const char* name, char* dest, unsigned max_dest_len, char* src, cmd_ctx_t * ctx);
bool        at_esp_config_get_set_u16(const char* name, uint16_t* dest, char* src, cmd_ctx_t * ctx);
void        at_esp_boot(char* args, cmd_ctx_t * ctx);
void        at_esp_reset(char* args, cmd_ctx_t * ctx);
