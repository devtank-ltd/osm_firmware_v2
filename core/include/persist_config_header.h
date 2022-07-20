#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"
#include "pinmap.h"


#define FLASH_ADDRESS               0x8000000
#define FLASH_PAGE_SIZE             2048

#if defined(STM32L451RE)
    #define FLASH_PAGE_NUMBER       256
#elif defined(STM32L433VTC6)
    #define FLASH_PAGE_NUMBER       128
#endif

#define FLASH_SIZE                  (FLASH_PAGE_NUMBER * FLASH_PAGE_SIZE)

#define FLASH_CONFIG_PAGE           (FLASH_PAGE_NUMBER - 1)
#define FW_PAGE                     2
#define NEW_FW_PAGE                 118

#define FW_PAGES                    100
#define FW_MAX_SIZE                 (FW_PAGES * FLASH_PAGE_SIZE)

#define PAGE2ADDR(_page_)           (FLASH_ADDRESS + (FLASH_PAGE_SIZE * _page_))
#define FW_ADDR                     PAGE2ADDR(FW_PAGE)
#define NEW_FW_ADDR                 PAGE2ADDR(NEW_FW_PAGE)
#define PERSIST_RAW_DATA            ((const uint8_t*)PAGE2ADDR(FLASH_CONFIG_PAGE))

#define PERSIST_VERSION            1


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    uint32_t                mins_interval;
    modbus_bus_t            modbus_bus;
    char                    lw_dev_eui[LW_DEV_EUI_LEN];
    char                    lw_app_key[LW_APP_KEY_LEN];
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
    uint32_t                cc_midpoints[ADC_CC_COUNT];
    uint8_t                 _[16-((ADC_CC_COUNT * sizeof(uint32_t))%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ___[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    cc_config_t             cc_configs[ADC_CC_COUNT];
} __attribute__((__packed__)) persist_storage_t;

_Static_assert(sizeof(persist_storage_t) <= FLASH_PAGE_SIZE, "Persistent memory too large.");
