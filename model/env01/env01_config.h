#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "cc.h"

#define __MODEL_CONFIG__

#define ENV01_FLASH_ADDRESS               0x8000000
#define ENV01_FLASH_PAGE_SIZE             2048

#define ENV01_FLASH_CONFIG_PAGE           2
#define ENV01_FLASH_MEASUREMENTS_PAGE     3
#define ENV01_FW_PAGE                     4
#define ENV01_NEW_FW_PAGE                 120

#define ENV01_FW_PAGES                    100
#define ENV01_FW_MAX_SIZE                 (ENV01_FW_PAGES * ENV01_FLASH_PAGE_SIZE)
#define ENV01_PAGE2ADDR(_page_)           (ENV01_FLASH_ADDRESS + (ENV01_FLASH_PAGE_SIZE * _page_))
#define ENV01_FW_ADDR                     ENV01_PAGE2ADDR(ENV01_FW_PAGE)
#define ENV01_NEW_FW_ADDR                 ENV01_PAGE2ADDR(ENV01_NEW_FW_PAGE)
#define ENV01_PERSIST_RAW_DATA            ((const uint8_t*)ENV01_PAGE2ADDR(ENV01_FLASH_CONFIG_PAGE))
#define ENV01_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)ENV01_PAGE2ADDR(ENV01_FLASH_MEASUREMENTS_PAGE))

#define ENV01_PERSIST_VERSION             3

#define ENV01_PERSIST_MODEL_CONFIG_T      persist_env01_config_v1_t

#define ENV01_MODEL_NAME                  "ENV01"

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128


typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    modbus_bus_t            modbus_bus;
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
    cc_config_t             cc_configs[ADC_CC_COUNT];
    uint8_t                 __[16-(ADC_CC_COUNT * sizeof(cc_config_t)%16)];
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 ___[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ____[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    /* 16 byte boundary ---- */
    /* 6 x 16 bytes          */
} persist_env01_config_v1_t;
