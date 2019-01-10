#ifndef __CONFIG__
#define __CONFIG__

#define CMD_LINELEN 32
#define LOG_LINELEN 32

#ifndef DOXYGEN
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)  __attribute__ ((format (printf, _fmt_arg, _el_arg)))
#else
#define PRINTF_FMT_CHECK(_fmt_arg, _el_arg)
#endif

#endif //__CONFIG__
