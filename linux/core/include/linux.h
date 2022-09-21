#pragma once

#include <stdbool.h>

bool linux_write_pty(unsigned index, char *data, int size);

void uart_debug_cb(char* in, unsigned len) __attribute__((weak));
void uart_lw_cb(char* in, unsigned len)  __attribute__((weak));
void uart_hpm_cb(char* in, unsigned len)  __attribute__((weak));
void uart_rs485_cb(char* in, unsigned len)  __attribute__((weak));

void i2c_linux_deinit(void) __attribute__((weak));

void sys_tick_handler(void) __attribute__((weak));
