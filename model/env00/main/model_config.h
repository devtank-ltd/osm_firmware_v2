#pragma once

#include "model_pinmap.h"

#define ENV00_PERSIST_VERSION             3

#define ENV00_PERSIST_MODEL_CONFIG_T      persist_env00_config_v1_t

#define ENV00_FLASH_PAGE_SIZE             2048
#define ENV00_MODEL_NAME                  "ENV00"

#define ENV00_FW_ADDR                     NULL
#define ENV00_NEW_FW_ADDR                 NULL
#define ENV00_PERSIST_RAW_DATA            ((const uint8_t*)NULL)
#define ENV00_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)NULL)

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512


typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    modbus_bus_t            modbus_bus;
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 ___[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
} persist_env00_config_v1_t;
