#ifndef __UART_RINGS__
#define __UART_RINGS__

extern void uart_ring_in(unsigned uart, const char* s, unsigned len);

extern void uart_ring_out(unsigned uart, const char* s, unsigned len);

extern void uart_rings_drain();


#endif //__UART_RINGS__
