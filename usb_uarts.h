#ifndef __USB__
#define __USB__

extern void     usb_init();
extern unsigned usb_uart_send(unsigned uart, void * data, unsigned len);

#endif //__USB__
