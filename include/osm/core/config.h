#pragma once

#include <stddef.h>

#define OSM_LOG_LINELEN 128

#define OSM_MEASURE_NAME_LEN            4
#define OSM_MEASURE_NAME_NULLED_LEN     (OSM_MEASURE_NAME_LEN+1)

#define OSM_MODBUS_NAME_LEN             OSM_MEASURE_NAME_LEN

#define OSM_MODBUS_MEMORY_SIZE 1024

#define OSM_MODBUS_RESP_TIMEOUT_MS 2000
#define OSM_MODBUS_SENT_TIMEOUT_MS 2000

/* On some versions of gcc this header isn't defining it. Quick fix. */
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIi64
#define PRIi64 "lli"
#endif
#ifndef PRId64
#define PRId64 "lld"
#endif

#ifndef DOXYGEN
#define OSM_PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define OSM_PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif


#define OSM_ADC_MAX_VAL       4095
#define OSM_ADCS_NUM_SAMPLES  1500
#define OSM_CC_DEFAULT_MIDPOINT                 (1000 * (OSM_ADC_MAX_VAL + 1) / 2 - 1)
#define OSM_CC_DEFAULT_EXT_MAX_MA               (100 * 1000)
#define OSM_CC_DEFAULT_INT_MAX_MV               50

#define OSM_FTMA_NUM_COEFFS      4

#define OSM_SAI_NUM_CAL_COEFFS   5

#define OSM_UART1_PRIORITY 3
#define OSM_UART2_PRIORITY 1
#define OSM_UART3_PRIORITY 1
#define OSM_UART4_PRIORITY 0
#define OSM_ADC_PRIORITY   DMA_CCR_PL_LOW
#define OSM_USB_PRIORITY   2
#define OSM_TIMER1_PRIORITY 2
#define OSM_TIMER2_PRIORITY 1
#define OSM_PPS_PRIORITY 1

#define OSM_PROTOCOL_HEX_ARRAY_SIZE 117

#define OSM_CMD_VUART 0
#define OSM_UART_ERR_NU 0

#define CONCAT2(a, b) a##b
#define OSM_CONCAT(a, b) CONCAT2(a, b)

#define OSM_MIN(a,b) (((a)<(b))?(a):(b))
#define OSM_MAX(a,b) (((a)>(b))?(a):(b))

#ifndef ARRAY_SIZE
#define OSM_ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))
#endif

#define OSM_MODBUS_BLOB_VERSION 2

#define ALIGN_TO(_x, _y) ((_x + _y -1 ) & ~(_y - 1)) ///< Align one number to another, for instance 16 for optimial addressing.
#define ALIGN_16(_x) ALIGN_TO(_x, 16)                ///< Align given number to 16.

#define GETOFFSET(_struct, _member) (uintptr_t)&((_struct*)NULL)->_member

#define STR_EXPAND(tok) #tok            ///< Convert macro value to a string.
#define STR(tok) STR_EXPAND(tok)        ///< Convert macro value to a string.
#define OSM_STRLEN(x) (sizeof(x) / sizeof(char) - 1)

#define OSM_STATIC_ASSERT_16BYTE_ALIGNED(_type_, _member_)  _Static_assert((offsetof(_type_, _member_) % 16) == 0, STR(_type_)"->"STR(_member_)" not on 16 byte boundary")

#define OSM_VEML7700_DEVTANK_CORRECTED 1

#define OSM_LOG_START_SPACER  "============{"
#define OSM_LOG_END_SPACER    "}============"
#define OSM_LOG_SPACER        "============="

#define OSM_USB_DATA_PCK_SZ    64

#define OSM_DMA_DATA_PCK_SZ    64

#define OSM_DEBUG_SYS             0x1
#define OSM_DEBUG_ADC             0x2
#define OSM_DEBUG_COMMS           0x4
#define OSM_DEBUG_IO              0x8
#define DEBUG_UART(_x_)     (0x10 << _x_) /*There is 4 uarts, so 4 bits, 0x10, 0x20, 0x40, 0x80*/
#define OSM_DEBUG_PARTICULATE   0x100
#define OSM_DEBUG_MODBUS        0x200
#define OSM_DEBUG_MEASUREMENTS  0x400
#define OSM_DEBUG_FW            0x800
#define OSM_DEBUG_TMP_HUM      0x1000
#define OSM_DEBUG_PULSECOUNT   0x2000
#define OSM_DEBUG_EXTTEMP      0x4000
#define OSM_DEBUG_LIGHT        0x8000
#define OSM_DEBUG_SOUND        0x10000
#define OSM_DEBUG_SLEEP        0x20000
#define OSM_DEBUG_CAN          0x40000
/* #define DEBUG_UNUSED       0x80000 */
#define OSM_DEBUG_CUSTOM_0    0x100000
#define OSM_DEBUG_CUSTOM_1    0x200000

#define __ADC_RMS_FULL__

#define OSM_IWDG_NORMAL_TIME_MS 10000
#define OSM_IWDG_MAX_TIME_MS    32760
#define OSM_SLEEP_MAX_TIME_MS   30000
