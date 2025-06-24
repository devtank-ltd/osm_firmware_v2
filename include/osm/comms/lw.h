#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <osm/core/base_types.h>

#define OSM_LW_UNSOL_VERSION                    0x01
#define OSM_LW_CONFIG_VERSION                   0x02

#define OSM_LW_ID_CMD_LEN                       4
#define OSM_LW_ID_CMD                           0x434d4400 /* CMD  */
#define OSM_LW_ID_CCMD                          0x43434d44 /* CCMD */
#define OSM_LW_ID_FW_START                      0x46572d00 /* FW-  */
#define OSM_LW_ID_FW_CHUNK                      0x46572b00 /* FW+  */
#define OSM_LW_ID_FW_COMPLETE                   0x46574000 /* FW@  */


#define OSM_LW_DEV_EUI_LEN                      16
#define OSM_LW_APP_KEY_LEN                      32
#define OSM_LW_REGION_LEN                       7


#define OSM_LW_REGION_NAME_EU433                "EU433\x00\x00"
#define OSM_LW_REGION_NAME_CN470                "CN470\x00\x00"
#define OSM_LW_REGION_NAME_RU864                "RU864\x00\x00"
#define OSM_LW_REGION_NAME_IN865                "IN865\x00\x00"
#define OSM_LW_REGION_NAME_EU868                "EU868\x00\x00"
#define OSM_LW_REGION_NAME_US915                "US915\x00\x00"
#define OSM_LW_REGION_NAME_AU915                "AU915\x00\x00"
#define OSM_LW_REGION_NAME_KR920                "KR920\x00\x00"
#define OSM_LW_REGION_NAME_AS923_1              "AS923-1"
#define OSM_LW_REGION_NAME_AS923_2              "AS923-2"
#define OSM_LW_REGION_NAME_AS923_3              "AS923-3"
#define OSM_LW_REGION_NAME_AS923_4              "AS923-4"


typedef enum
{
    OSM_LW_REGION_EU433    = 0 ,
    OSM_LW_REGION_CN470    = 1 ,
    OSM_LW_REGION_RU864    = 2 ,
    OSM_LW_REGION_IN865    = 3 ,
    OSM_LW_REGION_EU868    = 4 ,
    OSM_LW_REGION_US915    = 5 ,
    OSM_LW_REGION_AU915    = 6 ,
    OSM_LW_REGION_KR920    = 7 ,
    OSM_LW_REGION_AS923_1  = 8 ,
    OSM_LW_REGION_AS923_2  = 9 ,
    OSM_LW_REGION_AS923_3  = 10,
    OSM_LW_REGION_AS923_4  = 11,
    OSM_LW_REGION_MAX      = OSM_LW_REGION_AS923_4,
} osm_lw_region_t;


typedef struct
{
    uint8_t type;
    uint8_t _[15];
    char    dev_eui[OSM_LW_DEV_EUI_LEN];
    char    app_key[OSM_LW_APP_KEY_LEN];
    uint8_t region; /* osm_lw_region_t */
    uint8_t version;
} __attribute__((__packed__)) osm_lw_config_t;

OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_lw_config_t, dev_eui);
OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_lw_config_t, app_key);

_Static_assert(sizeof(osm_lw_config_t) < sizeof(osm_comms_config_t), "LoRaWAN config too big.");


bool            osm_lw_get_id(char* str, uint8_t len);
osm_lw_config_t*    osm_lw_get_config(void);
bool            osm_lw_persist_data_is_valid(void);
bool            osm_lw_config_setup_str(char * str, osm_cmd_ctx_t * ctx);
uint64_t        osm_lw_consume(char *p, unsigned len);
void            osm_lw_config_init(osm_comms_config_t* config);
bool            osm_lw_persist_config_cmp(osm_lw_config_t* d0, osm_lw_config_t* d1);
void            osm_lw_print_config(osm_cmd_ctx_t * ctx);
