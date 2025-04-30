#pragma once

#include <stdint.h>

#include <osm/core/measurements.h>
#include <osm/core/config.h>
#include "pinmap.h"
#include "esp_comms.h"

#define PERSIST_MODEL_VERSION       3
#define PERSIST_VERSION             PERSIST_VERSION_SET(PERSIST_MODEL_VERSION)

#define FLASH_PAGE_SIZE             2048

#define FW_ADDR                     NULL
#define NEW_FW_ADDR                 NULL

#define JSON_BUF_SIZE  1024

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define EXT_UART    1

#define MODBUS_SPEED        9600
#define MODBUS_PARITY       uart_parity_none
#define MODBUS_DATABITS     8
#define MODBUS_STOP         uart_stop_bits_1

#define ADC_MAX_MV          3300


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
} persist_model_config_v1_t;

#define persist_model_config_t        persist_model_config_v1_t
