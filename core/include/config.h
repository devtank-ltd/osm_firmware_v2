#pragma once

#define LOG_LINELEN 64

#define MEASURE_NAME_LEN            4
#define MEASURE_NAME_NULLED_LEN     (MEASURE_NAME_LEN+1)

#define MODBUS_NAME_LEN             MEASURE_NAME_LEN

#define MODBUS_MEMORY_SIZE 1024

/* On some versions of gcc this header isn't defining it. Quick fix. */
#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef DOXYGEN
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif


#define ADC_MAX_VAL       4095
#define ADC_MAX_MV        3300
#define ADCS_NUM_SAMPLES  1500
#define CC_DEFAULT_MIDPOINT                 (1000 * (ADC_MAX_VAL + 1) / 2 - 1)
#define CC_DEFAULT_EXT_MAX_MA               (100 * 1000)
#define CC_DEFAULT_INT_MAX_MV               50

#define FTMA_NUM_COEFFS      4

#define SAI_NUM_CAL_COEFFS   5

#define UART1_PRIORITY 3
#define UART2_PRIORITY 1
#define UART3_PRIORITY 1
#define UART4_PRIORITY 0
#define ADC_PRIORITY   DMA_CCR_PL_LOW
#define USB_PRIORITY   2
#define TIMER1_PRIORITY 2
#define TIMER2_PRIORITY 1
#define PPS_PRIORITY 1

#define PROTOCOL_HEX_ARRAY_SIZE 117

#define CMD_VUART 0
#define UART_ERR_NU 0

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define MODBUS_BLOB_VERSION 2

#define ALIGN_TO(_x, _y) ((_x + _y -1 ) & ~(_y - 1)) ///< Align one number to another, for instance 16 for optimial addressing.
#define ALIGN_16(_x) ALIGN_TO(_x, 16)                ///< Align given number to 16.

#define STR_EXPAND(tok) #tok            ///< Convert macro value to a string.
#define STR(tok) STR_EXPAND(tok)        ///< Convert macro value to a string.
#define STRLEN(x) (sizeof(x) / sizeof(char) - 1)

#define VEML7700_DEVTANK_CORRECTED 1

#define LOG_START_SPACER  "============{"
#define LOG_END_SPACER    "}============"
#define LOG_SPACER        "============="

/* Uart index on uart ring buffer use

    CMD_UART   0
    LW_UART    1
    HPM_UART   2
    EXT_UART 3
*/

/* Uart Index on STM Uart */

#define UART_4_SPEED 9600

#define UART_1_PARITY uart_parity_none
#define UART_2_PARITY uart_parity_none
#define UART_3_PARITY uart_parity_none
#define UART_4_PARITY uart_parity_none

#define UART_1_DATABITS 8
#define UART_2_DATABITS 8
#define UART_3_DATABITS 8
#define UART_4_DATABITS 8

#define UART_1_STOP uart_stop_bits_1
#define UART_2_STOP uart_stop_bits_1
#define UART_3_STOP uart_stop_bits_1
#define UART_4_STOP uart_stop_bits_1

#define MODBUS_SPEED    UART_4_SPEED
#define MODBUS_PARITY   UART_4_PARITY
#define MODBUS_DATABITS UART_4_DATABITS
#define MODBUS_STOP     UART_4_STOP

#define USB_DATA_PCK_SZ    64

#define DMA_DATA_PCK_SZ    64

#define DEBUG_SYS             0x1
#define DEBUG_ADC             0x2
#define DEBUG_COMMS           0x4
#define DEBUG_IO              0x8
#define DEBUG_UART(_x_)     (0x10 << _x_) /*There is 4 uarts, so 4 bits, 0x10, 0x20, 0x40, 0x80*/
#define DEBUG_HPM           0x100
#define DEBUG_MODBUS        0x200
#define DEBUG_MEASUREMENTS  0x400
#define DEBUG_FW            0x800
#define DEBUG_TMP_HUM      0x1000
#define DEBUG_PULSECOUNT   0x2000
#define DEBUG_EXTTEMP      0x4000
#define DEBUG_LIGHT        0x8000
#define DEBUG_SOUND        0x10000
#define DEBUG_SLEEP        0x20000
#define DEBUG_CAN          0x40000
#define DEBUG_MODE         0x80000
#define DEBUG_CUSTOM_0    0x100000
#define DEBUG_CUSTOM_1    0x200000

#define __ADC_RMS_FULL__

#define IWDG_NORMAL_TIME_MS 10000
#define IWDG_MAX_TIME_MS    32760
#define SLEEP_MAX_TIME_MS   30000

#define SERIAL_NUM_LEN         41
#define SERIAL_NUM_LEN_NULLED  (SERIAL_NUM_LEN + 1)
