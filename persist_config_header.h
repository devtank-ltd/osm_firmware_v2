#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"


#define FLASH_ADDRESS               0x8000000
#define FLASH_SIZE                  (512 * 1024)
#define FLASH_PAGE_SIZE             2048
#define FLASH_CONFIG_PAGE           255
#define FW_ADDR             (FLASH_ADDRESS + (FLASH_PAGE_SIZE * 2))
#define NEW_FW_ADDR         (FLASH_ADDRESS + (FLASH_PAGE_SIZE * 118))
#define FW_MAX_SIZE         (200*1024)
#define PERSIST__RAW_DATA           ((uint8_t*)(FLASH_ADDRESS + (FLASH_CONFIG_PAGE * FLASH_PAGE_SIZE)))
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

