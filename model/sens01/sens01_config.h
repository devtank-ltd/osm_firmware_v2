#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "ftma.h"

#define __MODEL_CONFIG__

#define SENS01_FLASH_ADDRESS               0x8000000
#define SENS01_FLASH_PAGE_SIZE             2048

#define SENS01_FLASH_CONFIG_PAGE           2
#define SENS01_FLASH_MEASUREMENTS_PAGE     3
#define SENS01_FW_PAGE                     4
#define SENS01_NEW_FW_PAGE                 120
#define SENS01_FW_PAGES                    100
#define SENS01_FW_MAX_SIZE                 (SENS01_FW_PAGES * SENS01_FLASH_PAGE_SIZE)
#define SENS01_PAGE2ADDR(_page_)           (SENS01_FLASH_ADDRESS + (SENS01_FLASH_PAGE_SIZE * _page_))
#define SENS01_FW_ADDR                     SENS01_PAGE2ADDR(SENS01_FW_PAGE)
#define SENS01_NEW_FW_ADDR                 SENS01_PAGE2ADDR(SENS01_NEW_FW_PAGE)

#define SENS01_PERSIST_RAW_DATA            ((const uint8_t*)SENS01_PAGE2ADDR(SENS01_FLASH_CONFIG_PAGE))
#define SENS01_PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)SENS01_PAGE2ADDR(SENS01_FLASH_MEASUREMENTS_PAGE))

#define SENS01_PERSIST_VERSION             3

#define SENS01_PERSIST_MODEL_CONFIG_T      persist_sens01_config_v1_t

#define SENS01_MODEL_NAME                  "SENS01"

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128


typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    modbus_bus_t            modbus_bus;
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
    ftma_config_t           ftma_configs[ADC_FTMA_COUNT];
    uint8_t                 __[16-(ADC_FTMA_COUNT * sizeof(ftma_config_t)%16)];
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 ___[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ____[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    /* 16 byte boundary ---- */
    /* 6 x 16 bytes          */
} persist_sens01_config_v1_t;

#define FTMA_RESISTOR_S_OHM                                 30
#define FTMA_RESISTOR_0_OHM                                 50000
#define FTMA_RESISTOR_G_OHM                                 12400
#define FTMA_HARDWARD_GAIN                                  (1.f / ((float)FTMA_RESISTOR_S_OHM * (((float)FTMA_RESISTOR_0_OHM / (float)FTMA_RESISTOR_G_OHM) + 1.f)))

#define FTMA_DEFAULT_COEFF_A                                  0.f
#define FTMA_DEFAULT_COEFF_B                                  FTMA_HARDWARD_GAIN
#define FTMA_DEFAULT_COEFF_C                                  0.f
#define FTMA_DEFAULT_COEFF_D                                  0.f

#define FTMA_DEFAULT_COEFFS                                 { FTMA_DEFAULT_COEFF_A , \
                                                              FTMA_DEFAULT_COEFF_B , \
                                                              FTMA_DEFAULT_COEFF_C , \
                                                              FTMA_DEFAULT_COEFF_D   }
