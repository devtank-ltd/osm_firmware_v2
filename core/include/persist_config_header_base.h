#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"


typedef struct
{
    uint32_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    char                    serial_number[SERIAL_NUM_LEN_NULLED];
} __attribute__((__packed__)) persist_storage_base_t;


typedef struct
{
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
} persist_measurements_storage_t;
