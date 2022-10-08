#pragma once

#include <stdbool.h>


#define LINUX_FILE_LOC          "/tmp/osm/"


extern volatile bool linux_threads_deinit;


bool linux_write_pty(unsigned index, const char *data, int size);

void uart_debug_cb(char* in, unsigned len) __attribute__((weak));
void uart_lw_cb(char* in, unsigned len)  __attribute__((weak));
void uart_hpm_cb(char* in, unsigned len)  __attribute__((weak));
void uart_rs485_cb(char* in, unsigned len)  __attribute__((weak));

void i2c_linux_deinit(void) __attribute__((weak));

void sys_tick_handler(void) __attribute__((weak));

void linux_adc_generate(void);
bool linux_kick_adc_gen(void);

void linux_port_debug(char * fmt, ...);
