#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"


#define FLASH_ADDRESS               0x8000000
#define FLASH_PAGE_SIZE             2048
#define FLASH_SIZE                  (256 * FLASH_PAGE_SIZE)

#define FLASH_CONFIG_PAGE           255
#define FW_PAGE                     2
#define NEW_FW_PAGE                 118

#define FW_PAGES                    100
#define FW_MAX_SIZE                 (FW_PAGES * FLASH_PAGE_SIZE)

#define PAGE2ADDR(_page_)           (FLASH_ADDRESS + (FLASH_PAGE_SIZE * _page_))
#define FW_ADDR                     PAGE2ADDR(FW_PAGE)
#define NEW_FW_ADDR                 PAGE2ADDR(NEW_FW_PAGE)
#define PERSIST_RAW_DATA            ((const uint8_t*)PAGE2ADDR(FLASH_CONFIG_PAGE))

#define PERSIST__VERSION            1


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    uint32_t                _;
    uint8_t                 modbus_data[MODBUS_MEMORY_SIZE];
    char                    lw_dev_eui[LW__DEV_EUI_LEN];
    char                    lw_app_key[LW__APP_KEY_LEN];
    measurement_def_base_t  measurements_arr[LW__MAX_MEASUREMENTS];
} __attribute__((__packed__)) persist_storage_t;

