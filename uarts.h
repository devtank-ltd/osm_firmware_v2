#ifndef __UARTS__
#define __UARTS__

extern void uarts_setup();

extern void uart_out(unsigned uart, const char* s, unsigned len);

extern bool uart_out_async(unsigned uart, char c);

#endif //__UARTS__
