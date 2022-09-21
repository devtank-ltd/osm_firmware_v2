#pragma once

#include <stdbool.h>


bool linux_add_pty(char* name, uint32_t* fd, void (*read_cb)(char *, unsigned int));
bool linux_add_gpio(char* name, uint32_t* fd, void (*cb)(uint32_t));

void uarts_linux_setup(void) __attribute__((weak));

void i2c_linux_deinit(void) __attribute__((weak));

void sys_tick_handler(void) __attribute__((weak));
