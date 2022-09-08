#pragma once


#define LW_DEV_EUI_LEN                      16
#define LW_APP_KEY_LEN                      32


typedef struct
{
    char    dev_eui[LW_DEV_EUI_LEN];
    char    app_key[LW_APP_KEY_LEN];
} lw_config_t;
