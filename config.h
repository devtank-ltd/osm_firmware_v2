#pragma once

#define CMD_LINELEN 64
#define LOG_LINELEN 64

#define MEASURE_NAME_LEN 4

#define MODBUS_NAME_LEN MEASURE_NAME_LEN


/* On some versions of gcc this header isn't defining it. Quick fix. */
#ifndef PRIu64
#define PRIu64 "llu"
#endif

#ifndef DOXYGEN
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif

#define ADCS_NUM_SAMPLES            480

#define UART1_PRIORITY 3
#define UART2_PRIORITY 1
#define UART3_PRIORITY 1
#define UART4_PRIORITY 0
#define ADC_PRIORITY   DMA_CCR_PL_LOW
#define USB_PRIORITY   2
#define TIMER1_PRIORITY 2
#define TIMER2_PRIORITY 1
#define PPS_PRIORITY 1


#define CMD_VUART 0
#define UART_ERR_NU 0

#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define MODBUS_MAX_DEV 4
#define MODBUS_DEV_REGS 16
#define MODBUS_BLOB_SIZE 16
#define MODBUS_REG_SIZE  16
#define MODBUS_DEV_SIZE   (8 + (MODBUS_REG_SIZE * MODBUS_DEV_REGS))
#define MODBUS_MEMORY_SIZE  (MODBUS_BLOB_SIZE + (MODBUS_DEV_SIZE * MODBUS_MAX_DEV))

#define ALIGN_TO(_x, _y) ((_x + _y -1 ) & ~(_y - 1)) ///< Align one number to another, for instance 16 for optimial addressing.
#define ALIGN_16(_x) ALIGN_TO(_x, 16)                ///< Align given number to 16.

#define STR_EXPAND(tok) #tok            ///< Convert macro value to a string.
#define STR(tok) STR_EXPAND(tok)        ///< Convert macro value to a string.

#define LOG_START_SPACER  "============{"
#define LOG_END_SPACER    "}============"
#define LOG_SPACER        "============="

/* Uart index on uart ring buffer use

    CMD_UART   0
    LW_UART    1
    HPM_UART   2
    RS485_UART 3
*/

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 2048

#define UART_1_IN_BUF_SIZE  128
#define UART_1_OUT_BUF_SIZE 512

#define UART_2_IN_BUF_SIZE  64
#define UART_2_OUT_BUF_SIZE 64

#define UART_3_IN_BUF_SIZE  128
#define UART_3_OUT_BUF_SIZE 128

/* Uart Index on STM Uart */

#define UART_1_SPEED 9600
#define UART_2_SPEED 115200
#define UART_3_SPEED 115200
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

#define USB_DATA_PCK_SZ    64

#define DMA_DATA_PCK_SZ    64

#define DEBUG_SYS             0x1
#define DEBUG_ADC             0x2
#define DEBUG_LW              0x4
#define DEBUG_IO              0x8
#define DEBUG_UART(_x_)     (0x10 << _x_) /*There is 4 uarts, so 4 bits, 0x10, 0x20, 0x40, 0x80*/
#define DEBUG_HPM           0x100
#define DEBUG_MODBUS        0x200
#define DEBUG_MEASUREMENTS  0x400
#define DEBUG_FW            0x800
#define DEBUG_TMP_HUM      0x1000
#define DEBUG_PULSECOUNT   0x2000

