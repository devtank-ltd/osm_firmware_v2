#pragma once

#include <stdint.h>


#include "measurements.h"
#include "config.h"
#include "persist_config_header_model.h"


#define MODEL_NAME_LEN                      16
#define HUMAN_NAME_LEN                      15
#define HUMAN_NAME_LEN_NULLED               (HUMAN_NAME_LEN + 1)


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
    char                    human_name[HUMAN_NAME_LEN_NULLED];
    uint8_t                 ____[16-(HUMAN_NAME_LEN_NULLED%16)];
    /* 16 byte boundary ---- */
    persist_model_config_t  model_config;
    /* 16 byte boundary ---- */
    uint64_t                config_count;
} __attribute__((__packed__)) persist_storage_t;


STATIC_ASSERT_16BYTE_ALIGNED(persist_storage_t, model_name);
STATIC_ASSERT_16BYTE_ALIGNED(persist_storage_t, serial_number);
STATIC_ASSERT_16BYTE_ALIGNED(persist_storage_t, human_name);
STATIC_ASSERT_16BYTE_ALIGNED(persist_storage_t, model_config);
STATIC_ASSERT_16BYTE_ALIGNED(persist_storage_t, config_count);


typedef struct
{
    measurements_def_t      measurements_arr[MEASUREMENTS_MAX_NUMBER];
} __attribute__((__packed__)) persist_measurements_storage_t;

