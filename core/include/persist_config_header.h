#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"
#include "persist_config_header_model.h"


#define MODEL_NAME_LEN                      16


typedef struct
{
    uint16_t                version;
    uint32_t                log_debug_mask;
    uint16_t                pending_fw:1;
    uint16_t                _reserved:15;
    uint8_t                 _[8];
    /* 16 byte boundary ---- */
    char                    model_name[MODEL_NAME_LEN];
    uint8_t                 __[16-(MODEL_NAME_LEN%16)];
    /* 16 byte boundary ---- */
    char                    serial_number[SERIAL_NUM_LEN_NULLED];
    uint8_t                 ___[16-(SERIAL_NUM_LEN_NULLED%16)];
    /* 16 byte boundary ---- */
    persist_model_config_t  model_config;
    /* 16 byte boundary ---- */
    uint64_t                config_count;
} __attribute__((__packed__)) persist_storage_t;


typedef struct
{
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
} persist_measurements_storage_t;

