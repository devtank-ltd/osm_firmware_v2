#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"
#include "persist_config_header_model.h"


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    char                    serial_number[SERIAL_NUM_LEN_NULLED];
    persist_model_config_t  model_config;
} __attribute__((__packed__)) persist_storage_t;


typedef struct
{
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
} persist_measurements_storage_t;


_Static_assert(sizeof(persist_storage_t) <= FLASH_PAGE_SIZE, "Persistent memory too large.");
_Static_assert(sizeof(persist_measurements_storage_t) <= FLASH_PAGE_SIZE, "Persistent measurements too large.");
