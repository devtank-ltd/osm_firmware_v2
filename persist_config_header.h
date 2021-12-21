#pragma once

#include <stdint.h>

#define FLASH_ADDRESS               FMC3_BANK_BASE
#define FLASH_SIZE                  (512 * 1024)
#define FLASH_PAGE_SIZE             2048
#define FLASH_CONFIG_PAGE           255
#define PERSIST__RAW_DATA           ((uint8_t*)(FLASH_ADDRESS + (FLASH_CONFIG_PAGE * FLASH_PAGE_SIZE)))
#define PERSIST__VERSION            1


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                fw_a;
    uint32_t                fw_b;
} __attribute__((__packed__)) persist_storage_header_t;

