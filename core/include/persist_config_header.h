#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"
#include "pinmap.h"


#define FLASH_ADDRESS               0x8000000
#define FLASH_PAGE_SIZE             2048

#define FLASH_MEASUREMENTS_PAGE     2
#define FLASH_CONFIG_PAGE           3
#define FW_PAGE                     4
#define NEW_FW_PAGE                 120

#define FW_PAGES                    100
#define FW_MAX_SIZE                 (FW_PAGES * FLASH_PAGE_SIZE)

#define PAGE2ADDR(_page_)           (FLASH_ADDRESS + (FLASH_PAGE_SIZE * _page_))
#define FW_ADDR                     PAGE2ADDR(FW_PAGE)
#define NEW_FW_ADDR                 PAGE2ADDR(NEW_FW_PAGE)
#define PERSIST_RAW_DATA            ((const uint8_t*)PAGE2ADDR(FLASH_CONFIG_PAGE))
#define PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)PAGE2ADDR(FLASH_MEASUREMENTS_PAGE))

#define PERSIST_VERSION             3


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    uint32_t                mins_interval;
    modbus_bus_t            modbus_bus;
    comms_config_t          comms_config;
    adc_persist_config_t    adc_persist_config;
    uint8_t                 _[16-(sizeof(adc_persist_config_t)%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ___[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    char                    serial_number[SERIAL_NUM_LEN_NULLED];
} __attribute__((__packed__)) persist_storage_t;


typedef struct
{
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
} persist_measurements_storage_t;

_Static_assert(sizeof(persist_storage_t) <= FLASH_PAGE_SIZE, "Persistent memory too large.");
_Static_assert(sizeof(persist_measurements_storage_t) <= FLASH_PAGE_SIZE, "Persistent measurements too large.");
