#ifndef __LOG__
#define __LOG__

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "value.h"

extern bool log_async_log;
extern uint32_t log_debug_mask;

extern void platform_raw_msg(const char * s);

extern void log_debug(uint32_t flag, const char * s, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_bad_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_out(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_init(void);

extern void log_debug_data(uint32_t flag, void * data, unsigned size);

extern void log_debug_value(uint32_t flag, const char * prefix, value_t * v);

#ifdef NOPODEBUG
#define log_sys_debug(...)      asm("nop")
#define adc_debug(...)          asm("nop")
#define lw_debug(...)           asm("nop")
#define io_debug(...)           asm("nop")
#define uart_debug(_uart_, ...) asm("nop")
#define hpm_debug(...)          asm("nop")
#define modbus_debug(...)       asm("nop")
#define measurements_debug(...) asm("nop")
#define fw_debug(...)           asm("nop")
#else
#define log_sys_debug(...)      log_debug(DEBUG_SYS, "SYS:" __VA_ARGS__)
#define adc_debug(...)          log_debug(DEBUG_ADC, "ADC: " __VA_ARGS__)
#define lw_debug(...)           log_debug(DEBUG_LW, "LORA: " __VA_ARGS__)
#define io_debug(...)           log_debug(DEBUG_IO, "IO: " __VA_ARGS__)
#define uart_debug(_uart_, ...) log_debug(DEBUG_UART(uart), "UART"STR(_uart_)": " __VA_ARGS__)
#define hpm_debug(...)          log_debug(DEBUG_HPM, "HPM: " __VA_ARGS__)
#define modbus_debug(...)       log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)
#define measurements_debug(...) log_debug(DEBUG_MEASUREMENTS, "Measure: " __VA_ARGS__)
#define fw_debug(...)           log_debug(DEBUG_FW, "FW: " __VA_ARGS__)
#endif

#endif //__LOG__
