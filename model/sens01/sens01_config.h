#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "ftma.h"

#define __MODEL_CONFIG__

#define SENS01_FLASH_ADDRESS               0x8000000
#define SENS01_FLASH_PAGE_SIZE             2048

#define SENS01_FLASH_MEASUREMENTS_PAGE     2
#define SENS01_FLASH_CONFIG_PAGE           3
#define SENS01_FW_PAGE                     4
#define SENS01_NEW_FW_PAGE                 120
#define SENS01_FW_PAGES                    100
#define SENS01_FW_MAX_SIZE                 (SENS01_FW_PAGES * SENS01_FLASH_PAGE_SIZE)
#define SENS01_PAGE2ADDR(_page_)           (SENS01_FLASH_ADDRESS + (SENS01_FLASH_PAGE_SIZE * _page_))
#define SENS01_FW_ADDR                     SENS01_PAGE2ADDR(SENS01_FW_PAGE)
#define SENS01_NEW_FW_ADDR                 SENS01_PAGE2ADDR(SENS01_NEW_FW_PAGE)

#define SENS01_PERSIST_RAW_DATA            ((const uint8_t*)SENS01_PAGE2ADDR(SENS01_FLASH_CONFIG_PAGE))
#define SENS01_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)SENS01_PAGE2ADDR(SENS01_FLASH_MEASUREMENTS_PAGE))

#define SENS01_PERSIST_VERSION             3

#define SENS01_PERSIST_MODEL_CONFIG_T      persist_sens01_config_v1_t

typedef struct
{
    uint32_t                mins_interval;
    modbus_bus_t            modbus_bus;
    comms_config_t          comms_config;
    ftma_config_t           ftma_configs[ADC_FTMA_COUNT];
    uint8_t                 _[16-(ADC_FTMA_COUNT * sizeof(ftma_config_t)%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
} persist_sens01_config_v1_t;

extern unsigned sens01_measurements_add_defaults(measurements_def_t * measurements_arr);
