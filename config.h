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
#define UART2_PRIORITY 2
#define UART3_PRIORITY 3
#define UART4_PRIORITY 0
#define USB_PRIORITY   1


#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))

#define LOG_START_SPACER  "============{"
#define LOG_END_SPACER    "}============"
#define LOG_SPACER        "============="

#define DEFAULT_SPS 8000

#define CMD_IN_BUF_SIZE  CMD_LINELEN
#define CMD_OUT_BUF_SIZE 1024

#define STD_UART_BUF_SIZE 256

#define USB_DATA_PCK_SZ    32

#endif //__CONFIG__
