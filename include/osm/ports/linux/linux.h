#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LINUX_PTY_NAME_STR_SIZE 32
#define LINUX_PTY_NAME_SIZE     (LINUX_PTY_NAME_STR_SIZE+1)
#define LOCATION_LEN 100

extern volatile bool linux_threads_deinit;

extern bool linux_has_reset;

bool osm_linux_write_pty(unsigned uart, const char *data, unsigned size);

void osm_uart_debug_cb(char* in, unsigned len) __attribute__((weak));
void osm_uart_lw_cb(char* in, unsigned len)  __attribute__((weak));
void osm_uart_hpm_cb(char* in, unsigned len)  __attribute__((weak));
void osm_uart_rs485_cb(char* in, unsigned len)  __attribute__((weak));

void osm_i2c_linux_deinit(void) __attribute__((weak));
void osm_w1_linux_deinit(void) __attribute__((weak));

void osm_sys_tick_handler(void) __attribute__((weak));

void osm_linux_adc_generate(void);
bool osm_linux_kick_adc_gen(void);

void osm_linux_port_debug(char * fmt, ...);

void osm_linux_usleep(unsigned usecs);
void osm_linux_awaken(void);
bool osm_socket_connect(char* path, int* _socketfd);

bool osm_peripherals_add_uart_tty_bridge(char * pty_name, unsigned uart);
void osm_linux_uart_proc(unsigned uart, char* in, unsigned len);

unsigned osm_linux_spawn(const char * rel_path);

void osm_linux_error(char* fmt, ...);
void osm_linux_error2(int error, char* fmt, ...);

char* osm_ret_static_file_location(void);
char osm_concat_osm_location(char* new_loc, unsigned loc_len, char* global);

int64_t osm_linux_get_current_us(void);

struct cmd_link_t* osm_linux_add_commands(struct cmd_link_t* tail);
