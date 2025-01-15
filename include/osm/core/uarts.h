#pragma once

#include <stdbool.h>
#include <osm/core/base_types.h>

void osm_uarts_setup();

void uart_enable(unsigned uart, bool enable);

void osm_uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_osm_uart_parity_t parity, osm_osm_uart_stop_bits_t stop, osm_cmd_ctx_t * ctx);
bool osm_uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_osm_uart_parity_t * parity, osm_osm_uart_stop_bits_t * stop);
bool osm_uart_resetup_str(unsigned uart, char * str, osm_cmd_ctx_t * ctx);

bool osm_uart_is_tx_empty(unsigned uart);

void osm_uart_blocking(unsigned uart, const char *data, unsigned size);

unsigned osm_uart_dma_out(unsigned uart, char *data, unsigned size);
