#pragma once

#include <stdint.h>


#include "persist_config_header_base.h"
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
    persist_storage_base_t  base;
    modbus_bus_t            modbus_bus;
    comms_config_t          comms_config;
    ftma_config_t           ftma_configs[ADC_FTMA_COUNT];
    uint8_t                 _[16-(ADC_FTMA_COUNT * sizeof(ftma_config_t)%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
} __attribute__((__packed__)) persist_storage_t;


_Static_assert(sizeof(persist_storage_t) <= FLASH_PAGE_SIZE, "Persistent memory too large.");
_Static_assert(sizeof(persist_measurements_storage_t) <= FLASH_PAGE_SIZE, "Persistent measurements too large.");
