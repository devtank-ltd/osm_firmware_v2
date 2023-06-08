#pragma once

#include <stdbool.h>
#include "base_types.h"

extern void uarts_setup();

extern void uart_enable(unsigned uart, bool enable);
extern bool uart_is_enabled(unsigned uart);

extern void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop);
extern bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_uart_parity_t * parity, osm_uart_stop_bits_t * stop);
extern bool uart_resetup_str(unsigned uart, char * str);

extern bool uart_is_tx_empty(unsigned uart);

extern void uart_blocking(unsigned uart, const char *data, int size);

extern bool uart_dma_out(unsigned uart, char *data, int size);
