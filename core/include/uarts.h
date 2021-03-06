#pragma once

#include <stdbool.h>
#include "pinmap.h"

extern void uarts_setup();

extern void uart_enable(unsigned uart, bool enable);
extern bool uart_is_enabled(unsigned uart);

extern void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop);
extern bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, uart_parity_t * parity, uart_stop_bits_t * stop);
extern bool uart_resetup_str(unsigned uart, char * str);

extern bool uart_is_tx_empty(unsigned uart);

extern bool uart_single_out(unsigned uart, char c);

extern void uart_blocking(unsigned uart, const char *data, int size);

extern bool uart_dma_out(unsigned uart, char *data, int size);
