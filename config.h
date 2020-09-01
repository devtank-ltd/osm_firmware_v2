#ifndef __CONFIG__
#define __CONFIG__

#define CMD_LINELEN 32
#define LOG_LINELEN 64

#ifndef DOXYGEN
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif

#define UART1_PRIORITY 3
#define UART2_PRIORITY 1
#define UART3_PRIORITY 0
#define UART4_PRIORITY 0
#define USB_PRIORITY   2
#define TIMER1_PRIORITY 2
#define TIMER2_PRIORITY 1
#define PPS_PRIORITY 1


#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define LOG_START_SPACER  "============{"
#define LOG_END_SPACER    "}============"
#define LOG_SPACER        "============="

#define DEFAULT_SPS 120000

#define CMD_OUT_BUF_SIZE 1024

#define UART_0_IN_BUF_SIZE  CMD_LINELEN
#define UART_0_OUT_BUF_SIZE 1024

#define UART_1_IN_BUF_SIZE  512
#define UART_1_OUT_BUF_SIZE 256

#define UART_2_IN_BUF_SIZE  256
#define UART_2_OUT_BUF_SIZE 256

#define UART_2_SPEED 115200
#define UART_3_SPEED 9600
#define UART_4_SPEED 115200

#define USB_DATA_PCK_SZ    64

#define DMA_DATA_PCK_SZ    32

#define DEBUG_SYS   0x1
#define DEBUG_ADC   0x2
#define DEBUG_UART  0x4
#define DEBUG_GPIO  0x8

#endif //__CONFIG__
