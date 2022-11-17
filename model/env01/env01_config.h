#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "cc.h"

#define ENV01_FLASH_ADDRESS               0x8000000
#define ENV01_FLASH_PAGE_SIZE             2048

#define ENV01_FLASH_MEASUREMENTS_PAGE     2
#define ENV01_FLASH_CONFIG_PAGE           3
#define ENV01_FW_PAGE                     4
#define ENV01_NEW_FW_PAGE                 120

#define ENV01_FW_PAGES                    100
#define ENV01_FW_MAX_SIZE                 (FW_PAGES * FLASH_PAGE_SIZE)
#define ENV01_PAGE2ADDR(_page_)           (FLASH_ADDRESS + (FLASH_PAGE_SIZE * _page_))
#define ENV01_FW_ADDR                     PAGE2ADDR(FW_PAGE)
#define ENV01_NEW_FW_ADDR                 PAGE2ADDR(NEW_FW_PAGE)
#define ENV01_PERSIST_RAW_DATA            ((const uint8_t*)PAGE2ADDR(FLASH_CONFIG_PAGE))
#define ENV01_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)PAGE2ADDR(FLASH_MEASUREMENTS_PAGE))

#define ENV01_PERSIST_VERSION             3


typedef struct
{
    uint32_t                mins_interval;
    modbus_bus_t            modbus_bus;
    comms_config_t          comms_config;
    cc_config_t             cc_configs[ADC_CC_COUNT];
    uint8_t                 _[16-(ADC_CC_COUNT * sizeof(cc_config_t)%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
} persist_env01_config_t;
