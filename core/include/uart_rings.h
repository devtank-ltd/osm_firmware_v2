#pragma once

#include <stdbool.h>
#include "ring.h"
#include "persist_config_header.h"

extern unsigned uart_ring_in(unsigned uart, const char* s, unsigned len);
extern unsigned uart_ring_out(unsigned uart, const char* s, unsigned len);

extern bool uart_ring_out_busy(unsigned uart);
extern bool uart_rings_out_busy(void);

extern void uart_ring_in_drain(unsigned uart);
extern void uart_rings_in_drain();
extern void uart_rings_out_drain();

extern void uart_rings_check();

extern void uart_rings_init(void);

extern char line_buffer[CMD_LINELEN];
