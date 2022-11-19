#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "cc.h"
#include "ftma.h"

#define PERSIST_VERSION  1
#define FLASH_PAGE_SIZE 2048
#define FW_MAX_SIZE (1024*100)
#define NEW_FW_ADDR 0x800000
#define NEW_FW_PAGE 100

typedef struct
{
    uint32_t                mins_interval;
    modbus_bus_t            modbus_bus;
    comms_config_t          comms_config;
    cc_config_t             cc_configs[ADC_CC_COUNT];
    uint8_t                 _[16-(ADC_CC_COUNT * sizeof(cc_config_t)%16)];
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 __[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    ftma_config_t           ftma_configs[ADC_FTMA_COUNT];
} persist_model_config_t;

