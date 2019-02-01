#include <stdlib.h>
#include <stdint.h>

#include "uart_rings.h"
#include "ring.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"

static ring_buf_t ring_in_bufs[4] = {RING_BUF_INIT,
                                     RING_BUF_INIT,
                                     RING_BUF_INIT,
                                     RING_BUF_INIT};

static ring_buf_t ring_out_bufs[4] = {RING_BUF_INIT,
                                      RING_BUF_INIT,
                                      RING_BUF_INIT,
                                      RING_BUF_INIT};

typedef char usb_packet_t[USB_DATA_PCK_SZ];
static usb_packet_t usb_out_packets[3];

static char command[CMD_LINELEN];


void uart_ring_in(unsigned uart, const char* s, unsigned len)
{
    if (uart > 3)
        return;

    ring_buf_t * ring = &ring_in_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        ring_buf_add(ring, s[n]);
}


void uart_ring_out(unsigned uart, const char* s, unsigned len)
{
    if (uart > 3)
        return;

    ring_buf_t * ring = &ring_out_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        ring_buf_add(ring, s[n]);
}


static unsigned _uart_out_async(char * c, unsigned len __attribute((unused)), void * puart)
{
    unsigned uart = *(unsigned*)puart;
    return uart_out_async(uart, *c)?1:0;
}


static unsigned _usb_out_async(char * tmp, unsigned len, void * puart)
{
    unsigned uart = *(unsigned*)puart;

    return usb_uart_send(uart-1, tmp, len);
}


void uart_rings_drain()
{
    for(unsigned n = 0; n < 4; n++)
    {
        ring_buf_t * ring = &ring_in_bufs[n];
        unsigned len = ring_buf_get_pending(ring);

        if (!len)
            continue;

        if (n)
        {
            char * tmp = usb_out_packets[n-1];

            len = (len > USB_DATA_PCK_SZ)?USB_DATA_PCK_SZ:len;

            ring_buf_consume(ring, _usb_out_async, tmp, len, &n);
        }
        else
        {
            unsigned len = ring_buf_readline(ring, command, CMD_LINELEN);

            if (len)
                cmds_process(command, len);
        }
    }

    for(unsigned n = 0; n < 4; n++)
    {
        ring_buf_t * ring = &ring_out_bufs[n];

        unsigned len = ring_buf_get_pending(ring);

        if (!len)
            continue;

        char c;
        ring_buf_consume(ring, _uart_out_async, &c, 1, &n);
    }
}


void uart_rings_check()
{
    for(unsigned n = 0; n < 4; n++)
    {
        ring_buf_t * in_ring  = &ring_in_bufs[n];
        ring_buf_t * out_ring = &ring_out_bufs[n];

        if (ring_buf_is_full(in_ring))
            log_error("UART %u in ring buffer filled.", n);
        else if (ring_buf_is_full(out_ring))
            log_error("UART %u out ring buffer filled.", n);
    }
}
