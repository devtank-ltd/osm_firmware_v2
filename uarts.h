#ifndef __UARTS__
#define __UARTS__

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

static inline char    uart_parity_as_char(uart_parity_t parity)    { return (parity == uart_parity_even)?'E':(parity == uart_parity_odd)?'O':'N'; }
static inline const char* uart_stop_bits_as_str(uart_stop_bits_t stop) { switch(stop) { case uart_stop_bits_1: return "1"; case uart_stop_bits_1_5: return "1.5"; case uart_stop_bits_2: return "2"; default: return "?"; } }

#endif //__UARTS__
