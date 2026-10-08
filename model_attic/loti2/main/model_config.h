#pragma once

#include <stdint.h>

#include <osm/core/measurements.h>
#include <osm/core/config.h>
#include "pinmap.h"
#include "rp2350_comms.h"

#define PERSIST_MODEL_VERSION       3
#define PERSIST_VERSION             OSM_PERSIST_VERSION_SET(PERSIST_MODEL_VERSION)

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

#define UART_1_IN_BUF_SIZE  CMD_LINELEN
#define UART_1_OUT_BUF_SIZE 2048

#define REAL_UART           0
#define CMD_UART            1

#define UART_0_SPEED        115200  /* DEBUG */

#define UART_0_PARITY       osm_uart_parity_none

#define UART_0_DATABITS     8


#define UART_0_STOP         osm_uart_stop_bits_1

#define IOS_WATCH_COUNT 0
#define IOS_COUNT 13

typedef char io_name_t[16];

typedef struct
{
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 _[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
    io_name_t               ios_names[IOS_COUNT];
} __attribute__((__packed__)) osm_persist_model_config_v1_t;

#define osm_persist_model_config_t        osm_persist_model_config_v1_t
