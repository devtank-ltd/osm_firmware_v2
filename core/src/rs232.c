#include "uart_rings.h"
#include "pinmap.h"
#include "ring.h"
#include "log.h"

#ifdef RS232_UART

static unsigned _rs232_send(char* msg, unsigned len)
{
    return uart_ring_out(RS232_UART, msg, len);
}


void rs232_process(ring_buf_t * ring)
{
    char c;
    unsigned len = ring_buf_read(ring, &c, 1);
    while (len)
    {
        c += 1;
        _rs232_send(&c, 1);
        len = ring_buf_read(ring, &c, 1);
    }
}

#endif //RS232_UART
