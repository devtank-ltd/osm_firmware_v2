#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include "base_types.h"

#define LW_UNSOL_VERSION                    0x01
#define LW_CONFIG_VERSION                   0x01

#define LW_ID_CMD_LEN                       4
#define LW_ID_CMD                           0x434d4400 /* CMD  */
#define LW_ID_CCMD                          0x43434d44 /* CCMD */
#define LW_ID_FW_START                      0x46572d00 /* FW-  */
#define LW_ID_FW_CHUNK                      0x46572b00 /* FW+  */
#define LW_ID_FW_COMPLETE                   0x46574000 /* FW@  */


#define LW_DEV_EUI_LEN                      16
#define LW_APP_KEY_LEN                      32
#define LW_REGION_LEN                       7


#define LW_REGION_NAME_EU433                "EU433\x00\x00"
#define LW_REGION_NAME_CN470                "CN470\x00\x00"
#define LW_REGION_NAME_RU864                "RU864\x00\x00"
#define LW_REGION_NAME_IN865                "IN865\x00\x00"
#define LW_REGION_NAME_EU868                "EU868\x00\x00"
#define LW_REGION_NAME_US915                "US915\x00\x00"
#define LW_REGION_NAME_AU915                "AU915\x00\x00"
#define LW_REGION_NAME_KR920                "KR920\x00\x00"
#define LW_REGION_NAME_AS923_1              "AS923-1"
#define LW_REGION_NAME_AS923_2              "AS923-2"
#define LW_REGION_NAME_AS923_3              "AS923-3"
#define LW_REGION_NAME_AS923_4              "AS923-4"


typedef enum
{
    LW_REGION_EU433    = 0 ,
    LW_REGION_CN470    = 1 ,
    LW_REGION_RU864    = 2 ,
    LW_REGION_IN865    = 3 ,
    LW_REGION_EU868    = 4 ,
    LW_REGION_US915    = 5 ,
    LW_REGION_AU915    = 6 ,
    LW_REGION_KR920    = 7 ,
    LW_REGION_AS923_1  = 8 ,
    LW_REGION_AS923_2  = 9 ,
    LW_REGION_AS923_3  = 10,
    LW_REGION_AS923_4  = 11,
    LW_REGION_MAX      = LW_REGION_AS923_4,
} lw_region_t;


typedef struct
{
    uint8_t type;
    uint8_t _[3];
    char    dev_eui[LW_DEV_EUI_LEN];
    char    app_key[LW_APP_KEY_LEN];
    uint8_t region; /* lw_region_t */
    uint8_t version;
} __attribute__((__packed__)) lw_config_t;

_Static_assert(sizeof(lw_config_t) < sizeof(comms_config_t), "LoRaWAN config too big.");


bool            lw_get_id(char* str, uint8_t len);
lw_config_t*    lw_get_config(void);
bool            lw_persist_data_is_valid(void);
bool            lw_config_setup_str(char * str);
uint64_t        lw_consume(char *p, unsigned len);
void            lw_config_init(comms_config_t* config);
bool            lw_persist_config_cmp(lw_config_t* d0, lw_config_t* d1);
