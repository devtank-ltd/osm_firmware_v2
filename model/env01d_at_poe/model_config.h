#pragma once

#include <stdint.h>

#include "measurements.h"
#include "config.h"
#include "pinmap.h"
#include "cc.h"
#include "at_poe.h"
#include "e_24lc00t.h"

#define FLASH_ADDRESS               0x8000000
#define FLASH_PAGE_SIZE             2048

#define FLASH_CONFIG_PAGE           2
#define FLASH_MEASUREMENTS_PAGE     3
#define FW_PAGE                     4
#define NEW_FW_PAGE                 66

#define FW_PAGES                    62
#define FW_MAX_SIZE                 (FW_PAGES * FLASH_PAGE_SIZE)
#define PAGE2ADDR(_page_)           (FLASH_ADDRESS + (FLASH_PAGE_SIZE * _page_))
#define FW_ADDR                     PAGE2ADDR(FW_PAGE)
#define NEW_FW_ADDR                 PAGE2ADDR(NEW_FW_PAGE)
#define PERSIST_RAW_DATA            ((const uint8_t*)PAGE2ADDR(FLASH_CONFIG_PAGE))
#define PERSIST_RAW_MEASUREMENTS    ((const uint8_t*)PAGE2ADDR(FLASH_MEASUREMENTS_PAGE))

#define PERSIST_MODEL_VERSION       4
#define PERSIST_VERSION             PERSIST_VERSION_SET(PERSIST_MODEL_VERSION)

#define CMD_LINELEN 256

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  256
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128

/* Uart index on uart ring buffer use

    CMD_UART   0
    LW_UART    1
    HPM_UART   2
    EXT_UART 3
*/

/* Uart Index on STM Uart */

#define CMD_UART            0
#define COMMS_UART          1
#define RS232_UART          2
#define EXT_UART            3

#define UART_1_SPEED        9600    /* EXAMPLE_RS232 */
#define UART_2_SPEED        115200  /* DEBUG */
#define UART_3_SPEED        115200  /* COMMS */
#define UART_4_SPEED        9600    /* RS485 */

#define UART_1_PARITY       uart_parity_none
#define UART_2_PARITY       uart_parity_none
#define UART_3_PARITY       uart_parity_none
#define UART_4_PARITY       uart_parity_none

#define UART_1_DATABITS     8
#define UART_2_DATABITS     8
#define UART_3_DATABITS     8
#define UART_4_DATABITS     8

#define UART_1_STOP         uart_stop_bits_1
#define UART_2_STOP         uart_stop_bits_1
#define UART_3_STOP         uart_stop_bits_1
#define UART_4_STOP         uart_stop_bits_1

#define MODBUS_SPEED        UART_4_SPEED
#define MODBUS_PARITY       UART_4_PARITY
#define MODBUS_DATABITS     UART_4_DATABITS
#define MODBUS_STOP         UART_4_STOP

#define IOS_COUNT           3

#define ADC_MAX_MV          3300
#define ADC_CC_COUNT        3
#define CC_DEFAULT_TYPE     CC_TYPE_A

#define JSON_BUF_SIZE  256

#define COMMS_IDENTITY_DEFAULT  COMMS_TYPE_POE


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
    uint32_t                pulsecount_debounces_ms[W1_PULSE_COUNT];
    uint8_t                 ____[16-((W1_PULSE_COUNT * sizeof(uint32_t))%16)];
    /* 16 byte boundary ---- */
    float                   sai_cal_coeffs[SAI_NUM_CAL_COEFFS];
    uint8_t                 _____[16-((SAI_NUM_CAL_COEFFS * sizeof(float))%16)];
    /* 16 byte boundary ---- */
    uint32_t                sai_no_buf;
    uint8_t                 ______[16-(sizeof(uint32_t)%16)];
    /* 7 x 16 bytes          */
} persist_model_config_v1_t;

STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, modbus_bus);
STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, comms_config);
STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, cc_configs);
STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, ios_state);
STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, pulsecount_debounces_ms);
STATIC_ASSERT_16BYTE_ALIGNED(persist_model_config_v1_t, sai_cal_coeffs);

#define persist_model_config_t        persist_model_config_v1_t
