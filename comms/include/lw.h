#pragma once

#include <inttypes.h>
#include <stdbool.h>


#define LW_DEV_EUI_LEN                      16
#define LW_APP_KEY_LEN                      32


typedef struct
{
    char    dev_eui[LW_DEV_EUI_LEN];
    char    app_key[LW_APP_KEY_LEN];
} lw_config_t;


bool            lw_get_id(char* str, uint8_t len);
lw_config_t*    lw_get_config(void);
bool            lw_persist_data_is_valid(void);
bool            lw_config_setup_str(char * str);
