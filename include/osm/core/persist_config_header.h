#pragma once

#include <stdint.h>


#include <osm/core/measurements.h>
#include <osm/core/config.h>
#include "persist_config_header_model.h"


#define OSM_SERIAL_NUM_LEN         47
#define OSM_SERIAL_NUM_LEN_NULLED  (OSM_SERIAL_NUM_LEN + 1)

#define OSM_MODEL_NAME_LEN                      31
#define OSM_MODEL_NAME_LEN_NULLED               (OSM_MODEL_NAME_LEN + 1)

#define OSM_HUMAN_NAME_LEN                      31
#define OSM_HUMAN_NAME_LEN_NULLED               (OSM_HUMAN_NAME_LEN + 1)


typedef struct
{
    uint16_t                version;
    uint32_t                log_debug_mask;
    uint32_t                pending_fw;
    uint8_t                 _[6];
    /* 16 byte boundary ---- */
    char                    model_name[OSM_MODEL_NAME_LEN_NULLED];
    /* 16 byte boundary ---- */
    char                    serial_number[OSM_SERIAL_NUM_LEN_NULLED];
    /* 16 byte boundary ---- */
    char                    human_name[OSM_HUMAN_NAME_LEN_NULLED];
    /* 16 byte boundary ---- */
    osm_persist_model_config_t  model_config;
    /* 16 byte boundary ---- */
    uint64_t                config_count;
} __attribute__((__packed__)) osm_persist_storage_t;


OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_persist_storage_t, model_name);
OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_persist_storage_t, serial_number);
OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_persist_storage_t, human_name);
OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_persist_storage_t, model_config);
OSM_STATIC_ASSERT_16BYTE_ALIGNED(osm_persist_storage_t, config_count);


typedef struct
{
    osm_measurements_def_t      measurements_arr[OSM_MEASUREMENTS_MAX_NUMBER];
} __attribute__((__packed__)) osm_persist_measurements_storage_t;
