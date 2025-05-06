#pragma once

#include <stdbool.h>
#include <osm/core/ring.h>
#include <osm/core/persist_config_header.h>

unsigned uart_ring_in(unsigned uart, const char* s, unsigned len);
unsigned uart_ring_out(unsigned uart, const char* s, unsigned len);

unsigned uart_ring_in_get_len(unsigned uart);
unsigned uart_ring_out_get_len(unsigned uart);

bool uart_ring_out_busy(unsigned uart);
bool uart_rings_out_busy(void);

void uart_ring_in_drain(unsigned uart);
void uart_rings_in_drain();
void uart_rings_out_drain();
void uart_rings_drain_all_out(void);

void uart_rings_check();

void uart_rings_init(void);
void uart_rings_in_wipe(unsigned uart);
void uart_rings_out_wipe(unsigned uart);

extern char line_buffer[CMD_LINELEN];

extern cmd_ctx_t uart_cmd_ctx;
