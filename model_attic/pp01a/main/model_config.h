#pragma once

#include <stdint.h>

#include <osm/core/measurements.h>
#include <osm/core/config.h>
#include "pinmap.h"
#include "rp2350_comms.h"

#define PERSIST_MODEL_VERSION       3
#define PERSIST_VERSION             PERSIST_VERSION_SET(PERSIST_MODEL_VERSION)

/* TODO: Currently using last sectors, halved for each part, but will
 * move this when bootloader added.
 */
#define _MEM_SIZE_KB                (2048 * 1024)
#define PERSIST_CONFIG_SECTOR       (_MEM_SIZE_KB - FLASH_SECTOR_SIZE)
#define PERSIST_CONFIG_SIZE         (FLASH_PAGE_SIZE * 8) // = FLASH_SECTOR_SIZE / 2
#define PERSIST_RAW_DATA            ((uint8_t*)(XIP_BASE + PERSIST_CONFIG_SECTOR))
#define PERSIST_RAW_MEASUREMENTS    ((uint8_t*)(XIP_BASE + PERSIST_CONFIG_SECTOR + PERSIST_CONFIG_SIZE))

#define FW_ADDR                     NULL
#define NEW_FW_ADDR                 NULL

#define JSON_BUF_SIZE  1024

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

/* Uart index on uart ring buffer use

    CMD_UART    0
    COMMS_UART  1
*/

#define CMD_UART            0
#define COMMS_UART          1

#define UART_0_SPEED        115200  /* DEBUG */
#define UART_1_SPEED        9600    /* COMMS */

#define UART_0_PARITY       uart_parity_none
#define UART_1_PARITY       uart_parity_none

#define UART_0_DATABITS     8
#define UART_1_DATABITS     8

#define UART_0_STOP         uart_stop_bits_1
#define UART_1_STOP         uart_stop_bits_1

#define IOS_COUNT 0

typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
} persist_model_config_v1_t;

#define persist_model_config_t        persist_model_config_v1_t
