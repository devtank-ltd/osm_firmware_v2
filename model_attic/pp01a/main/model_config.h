#pragma once

#include <stdint.h>

#include <osm/core/measurements.h>
#include <osm/core/config.h>
#include "pinmap.h"
#include "rp2350_comms.h"

#define PERSIST_MODEL_VERSION       3
#define PERSIST_VERSION             PERSIST_VERSION_SET(PERSIST_MODEL_VERSION)

/* We have 2MB of flash. Given 32kB to the bootloader, 2 sectors
   reserved for config (8kB), leaving 2056kB for
   application code. As we want space for 2 full applications for
   updating, 1028kB maximum size each.
*/
#define BOOTLOADER_SIZE             (32 * 1024)

#define _SECTOR_TO_ADDR(_s)         (XIP_BASE + _s)
#define _ADDR_TO_SECTOR(_a)         (_a - XIP_BASE)
#define _SECTOR_TO_PAGE(_s)         (_s / FLASH_PAGE_SIZE)

#define PERSIST_CONFIG_SECTOR       BOOTLOADER_SIZE
#define PERSIST_CONFIG_SECTOR_ADDR  _SECTOR_TO_ADDR(PERSIST_CONFIG_SECTOR)
#define PERSIST_CONFIG_SIZE         (FLASH_PAGE_SIZE * 8) // = FLASH_SECTOR_SIZE / 2
#define PERSIST_RAW_DATA            ((uint8_t*)PERSIST_CONFIG_SECTOR_ADDR)
#define PERSIST_RAW_MEASUREMENTS    ((uint8_t*)(PERSIST_CONFIG_SECTOR_ADDR + PERSIST_CONFIG_SIZE))

#define FW_MAX_SIZE                 (1028 * 1024)
#define FW_SECTOR                   (PERSIST_CONFIG_SECTOR + 2 * FLASH_SECTOR_SIZE)
#define FW_ADDR                     _SECTOR_TO_ADDR(FW_SECTOR)
#define NEW_FW_SECTOR               (FW_SECTOR + FW_MAX_SIZE)
#define NEW_FW_ADDR                 _SECTOR_TO_ADDR(NEW_FW_SECTOR)
#define NEW_FW_PAGE                 _SECTOR_TO_PAGE(NEW_FW_SECTOR)

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
