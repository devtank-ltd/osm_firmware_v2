#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

extern bool log_async_log;
extern uint32_t log_debug_mask;

extern void platform_raw_msg(const char * s);

extern void log_debug(uint32_t flag, const char * s, ...) PRINTF_FMT_CHECK( 2, 3);

extern void log_bad_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_error(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_out(const char * s, ...) PRINTF_FMT_CHECK( 1, 2);
extern void log_init(void);

extern void log_debug_data(uint32_t flag, void * data, unsigned size);

#ifdef NOPODEBUG
inline static void _empty_log() {}
#define log_sys_debug(...)      _empty_log(__VA_ARGS__)
#define adc_debug(...)          _empty_log(__VA_ARGS__)
#define comms_debug(...)        _empty_log(__VA_ARGS__)
#define io_debug(...)           _empty_log(__VA_ARGS__)
#define uart_debug(_uart_, ...) _empty_log(__VA_ARGS__)
#define particulate_debug(...)  _empty_log(__VA_ARGS__)
#define modbus_debug(...)       _empty_log(__VA_ARGS__)
#define measurements_debug(...) _empty_log(__VA_ARGS__)
#define fw_debug(...)           _empty_log(__VA_ARGS__)
#define pulsecount_debug(...)   _empty_log(__VA_ARGS__)
#define exttemp_debug(...)      _empty_log(__VA_ARGS__)
#define light_debug(...)        _empty_log(__VA_ARGS__)
#define sound_debug(...)        _empty_log(__VA_ARGS__)
#define sleep_debug(...)        _empty_log(__VA_ARGS__)
#define can_debug(...)          _empty_log(__VA_ARGS__)
#define dm_debug(...)           _empty_log(__VA_ARGS__)
#define custom_0_debug(...)     _empty_log(__VA_ARGS__)
#define custom_1_debug(...)     _empty_log(__VA_ARGS__)
#else
#define log_sys_debug(...)      log_debug(DEBUG_SYS, "SYS:" __VA_ARGS__)
#define adc_debug(...)          log_debug(DEBUG_ADC, "ADC: " __VA_ARGS__)
#define comms_debug(...)        log_debug(DEBUG_COMMS, "COMMS: " __VA_ARGS__)
#define io_debug(...)           log_debug(DEBUG_IO, "IO: " __VA_ARGS__)
#define uart_debug(_uart_, ...) log_debug(DEBUG_UART(uart), "UART"STR(_uart_)": " __VA_ARGS__)
#define particulate_debug(...)  log_debug(DEBUG_PARTICULATE, "PART: " __VA_ARGS__)
#define modbus_debug(...)       log_debug(DEBUG_MODBUS, "Modbus: " __VA_ARGS__)
#define measurements_debug(...) log_debug(DEBUG_MEASUREMENTS, "Measure: " __VA_ARGS__)
#define fw_debug(...)           log_debug(DEBUG_FW, "FW: " __VA_ARGS__)
#define pulsecount_debug(...)   log_debug(DEBUG_PULSECOUNT, "PLSECNT: " __VA_ARGS__)
#define exttemp_debug(...)      log_debug(DEBUG_EXTTEMP, "EXTTEMP: "  __VA_ARGS__)
#define light_debug(...)        log_debug(DEBUG_LIGHT, "LIGHT: "  __VA_ARGS__)
#define sound_debug(...)        log_debug(DEBUG_SOUND, "SOUND: "  __VA_ARGS__)
#define sleep_debug(...)        log_debug(DEBUG_SLEEP, "SLEEP: "  __VA_ARGS__)
#define can_debug(...)          log_debug(DEBUG_CAN, "CAN: "  __VA_ARGS__)
#define dm_debug(...)           log_debug(DEBUG_MODE, "DEBUG:"  __VA_ARGS__)
#define custom_0_debug(...)     log_debug(DEBUG_CUSTOM_0, "CUSTOM_0:"  __VA_ARGS__)
#define custom_1_debug(...)     log_debug(DEBUG_CUSTOM_1, "CUSTOM_1:"  __VA_ARGS__)
#endif
