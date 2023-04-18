#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LINUX_FILE_LOC          "/tmp/osm/"

#define LINUX_PTY_NAME_SIZE     16
#define LOCATION_LEN 100

extern volatile bool linux_threads_deinit;

extern bool linux_has_reset;

bool linux_write_pty(unsigned uart, const char *data, unsigned size);

void uart_debug_cb(char* in, unsigned len) __attribute__((weak));
void uart_lw_cb(char* in, unsigned len)  __attribute__((weak));
void uart_hpm_cb(char* in, unsigned len)  __attribute__((weak));
void uart_rs485_cb(char* in, unsigned len)  __attribute__((weak));

void i2c_linux_deinit(void) __attribute__((weak));
void w1_linux_deinit(void) __attribute__((weak));

void sys_tick_handler(void) __attribute__((weak));

void linux_adc_generate(void);
bool linux_kick_adc_gen(void);

void linux_port_debug(char * fmt, ...);

void linux_usleep(unsigned usecs);
void linux_awaken(void);
bool socket_connect(char* path, int* _socketfd);

bool peripherals_add_uart_tty_bridge(char * pty_name, unsigned uart);
void linux_uart_proc(unsigned uart, char* in, unsigned len);

unsigned linux_spawn(const char * rel_path);

void linux_error(char* fmt, ...);

char* ret_static_file_location(void);
char concat_osm_location(char* new_loc, int loc_len, char* global);

int64_t linux_get_current_us(void);
