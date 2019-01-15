#ifndef __CONFIG__
#define __CONFIG__

#define CMD_LINELEN 32
#define LOG_LINELEN 64

#ifndef DOXYGEN
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif

#define UART1_PRIORITY 1
#define UART2_PRIORITY 2
#define UART3_PRIORITY 3
#define UART4_PRIORITY 4
#define USB_PRIORITY   5


#define ARRAY_SIZE(_a) (sizeof(_a)/sizeof(_a[0]))


#endif //__CONFIG__
