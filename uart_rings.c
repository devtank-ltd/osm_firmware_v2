#include <stdlib.h>
#include <stdint.h>

#include "uart_rings.h"
#include "ring.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"
#include "pinmap.h"


typedef char std_uart_buf_t[STD_UART_BUF_SIZE];

static char cmd_uart_buf_in[CMD_IN_BUF_SIZE];
static char cmd_uart_buf_out[CMD_OUT_BUF_SIZE];

static std_uart_buf_t std_uart_in_bufs[2];
static std_uart_buf_t std_uart_out_bufs[UART_CHANNELS_COUNT];

static ring_buf_t ring_in_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(cmd_uart_buf_in, sizeof(cmd_uart_buf_in)),
    RING_BUF_INIT(std_uart_in_bufs[0], STD_UART_BUF_SIZE),
    RING_BUF_INIT(std_uart_in_bufs[1], STD_UART_BUF_SIZE)};

static ring_buf_t ring_out_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(std_uart_out_bufs[0], STD_UART_BUF_SIZE),
    RING_BUF_INIT(std_uart_out_bufs[1], STD_UART_BUF_SIZE),
    RING_BUF_INIT(std_uart_out_bufs[2], STD_UART_BUF_SIZE)};

static ring_buf_t cmd_ring_out_buf = RING_BUF_INIT(cmd_uart_buf_out, CMD_OUT_BUF_SIZE);

typedef char usb_packet_t[USB_DATA_PCK_SZ];
static usb_packet_t usb_out_packets[UART_CHANNELS_COUNT];

static char command[CMD_LINELEN];


unsigned uart_ring_in(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_in_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(ring, s[n]))
            return n;
    return len;
}


unsigned uart_ring_out(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_out_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(ring, s[n]))
            return n;
    return len;
}


unsigned cmd_ring_out(const char* s, unsigned len)
{
    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(&cmd_ring_out_buf, s[n]))
            return n;
    return len;

}


static unsigned _uart_out_async(char * c, unsigned len __attribute((unused)), void * puart)
{
    unsigned uart = *(unsigned*)puart;
    return uart_out_async(uart, *c)?1:0;
}


static unsigned _usb_out_async(char * tmp, unsigned len, void * puart)
{
    unsigned uart = (puart)?(*(unsigned*)puart):0;

    return usb_uart_send(uart, tmp, len);
}


static void uart_ring_in_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_in_bufs[uart];
    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

    if (uart)
    {
        char * tmp = usb_out_packets[uart];

        len = (len > USB_DATA_PCK_SZ)?USB_DATA_PCK_SZ:len;

        ring_buf_consume(ring, _usb_out_async, tmp, len, &uart);
    }
    else
    {
        unsigned len = ring_buf_readline(ring, command, CMD_LINELEN);

        if (len)
            cmds_process(command, len);
    }
}


static void uart_ring_out_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_out_bufs[uart];

    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

    char c;
    ring_buf_consume(ring, _uart_out_async, &c, 1, &uart);
}


static void cmd_ring_out_drain()
{
    unsigned len = ring_buf_get_pending(&cmd_ring_out_buf);

    if (!len)
        return;

    len = (len > USB_DATA_PCK_SZ)?USB_DATA_PCK_SZ:len;

    ring_buf_consume(&cmd_ring_out_buf, _usb_out_async, usb_out_packets[0], len, NULL);
}


void uart_rings_in_drain()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_ring_in_drain(n);
}


void uart_rings_out_drain()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_ring_out_drain(n);
    cmd_ring_out_drain();
}


void uart_rings_check()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        ring_buf_t * in_ring  = &ring_in_bufs[n];
        ring_buf_t * out_ring = &ring_out_bufs[n];

        if (ring_buf_is_full(in_ring))
            log_debug("UART %u in ring buffer filled.", n);
        else if (ring_buf_is_full(out_ring))
            log_debug("UART %u out ring buffer filled.", n);
    }
}
