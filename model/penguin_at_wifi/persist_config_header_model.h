#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "cc.h"
#include "ftma.h"
#include "linux_comms.h"

#define PERSIST_VERSION  1
#define FLASH_PAGE_SIZE 2048
#define FW_MAX_SIZE (1024*100)
#define NEW_FW_ADDR 0x800000
#define NEW_FW_PAGE 100

#define CMD_LINELEN 128

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128

#define IOS_COUNT           10
#define ADC_CC_COUNT        3
#define ADC_FTMA_COUNT      4

#define comms_name      linux_comms

typedef struct
{
    uint32_t                mins_interval;
    uint8_t                 _[12];
    /* 16 byte boundary ---- */
    modbus_bus_t            modbus_bus;
    /* 16 byte boundary ---- */
    comms_config_t          comms_config;
    /* 16 byte boundary ---- */
    cc_config_t             cc_configs[ADC_CC_COUNT];
    uint8_t                 __[16-(ADC_CC_COUNT * sizeof(cc_config_t)%16)];
    /* 16 byte boundary ---- */
    uint16_t                ios_state[IOS_COUNT];
    uint8_t                 ___[16-((IOS_COUNT * sizeof(uint16_t))%16)];
    /* 16 byte boundary ---- */
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 ____[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    /* 16 byte boundary ---- */
    ftma_config_t           ftma_configs[ADC_FTMA_COUNT];
    uint8_t                 _____[16-((ADC_FTMA_COUNT * sizeof(ftma_config_t))%16)];
    /* 16 byte boundary ---- */
    uint32_t                sai_no_buf;
    uint8_t                 ______[16-(sizeof(uint32_t)%16)];
    /* 16 byte boundary ---- */
    /* 8 x 16 bytes          */
} persist_penguin_at_wifi_config_v1_t;

#define persist_model_config_t        persist_penguin_at_wifi_config_v1_t

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
