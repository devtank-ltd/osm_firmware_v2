#ifndef __LOG__
#define __LOG__

#include "config.h"

extern bool log_show_debug;
extern bool log_async_log;

extern void platform_raw_msg(const char * s);

extern void log_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_out(const char * s, ...)   PRINTF_FMT_CHECK( 1, 2);
extern void log_debug(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);

extern void log_debug_raw(const char *s, ...) PRINTF_FMT_CHECK( 1, 2);

extern void log_init();

#endif //__LOG__
