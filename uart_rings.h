#ifndef __UART_RINGS__
#define __UART_RINGS__

extern unsigned uart_ring_in(unsigned uart, const char* s, unsigned len);
extern unsigned uart_ring_out(unsigned uart, const char* s, unsigned len);

extern void uart_rings_in_drain();
extern void uart_rings_out_drain();

extern void uart_rings_check();


#endif //__UART_RINGS__
