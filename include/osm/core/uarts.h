#pragma once

#include <stdbool.h>
#include <osm/core/base_types.h>

void uarts_setup();

void uart_enable(unsigned uart, bool enable);
bool uart_is_enabled(unsigned uart);

void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop, cmd_ctx_t * ctx);
bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_uart_parity_t * parity, osm_uart_stop_bits_t * stop);
bool uart_resetup_str(unsigned uart, char * str, cmd_ctx_t * ctx);

bool uart_is_tx_empty(unsigned uart);

void uart_blocking(unsigned uart, const char *data, unsigned size);

unsigned uart_dma_out(unsigned uart, char *data, unsigned size);
