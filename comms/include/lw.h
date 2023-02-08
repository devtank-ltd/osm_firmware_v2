#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define LW_UNSOL_VERSION                    0x01

#define LW_ID_CMD_LEN                       4
#define LW_ID_CMD                           0x434d4400 /* CMD  */
#define LW_ID_CCMD                          0x43434d44 /* CCMD */
#define LW_ID_ERR_CODE                      "ERR"      /* ERR  */
#define LW_ID_FW_START                      0x46572d00 /* FW-  */
#define LW_ID_FW_CHUNK                      0x46572b00 /* FW+  */
#define LW_ID_FW_COMPLETE                   0x46574000 /* FW@  */


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
uint64_t        lw_consume(char *p, unsigned len);
