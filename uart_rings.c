#include <stdlib.h>
#include <stdint.h>

#include "pinmap.h"
#include "uart_rings.h"
#include "ring.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "usb_uarts.h"


typedef char usb_packet_t[USB_DATA_PCK_SZ];
typedef char dma_uart_buf_t[DMA_DATA_PCK_SZ];

char uart_0_in_buf[UART_0_IN_BUF_SIZE];
char uart_0_out_buf[UART_0_OUT_BUF_SIZE];

char uart_1_in_buf[UART_1_IN_BUF_SIZE];
char uart_1_out_buf[UART_1_OUT_BUF_SIZE];

char uart_2_in_buf[UART_2_IN_BUF_SIZE];
char uart_2_out_buf[UART_2_OUT_BUF_SIZE];

char cmd_uart_buf_out[CMD_OUT_BUF_SIZE];

static ring_buf_t ring_in_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(uart_0_in_buf, sizeof(uart_0_in_buf)),
    RING_BUF_INIT(uart_1_in_buf, sizeof(uart_1_in_buf)),
    RING_BUF_INIT(uart_2_in_buf, sizeof(uart_2_in_buf))};

static ring_buf_t ring_out_bufs[UART_CHANNELS_COUNT] = {
    RING_BUF_INIT(uart_0_out_buf, sizeof(uart_0_out_buf)),
    RING_BUF_INIT(uart_1_out_buf, sizeof(uart_1_out_buf)),
    RING_BUF_INIT(uart_2_out_buf, sizeof(uart_2_out_buf))};

static ring_buf_t cmd_ring_out_buf = RING_BUF_INIT(cmd_uart_buf_out, sizeof(cmd_uart_buf_out));

static usb_packet_t usb_out_packets[UART_CHANNELS_COUNT];

static char command[CMD_LINELEN];

static dma_uart_buf_t uart_dma_buf[UART_CHANNELS_COUNT];


unsigned uart_ring_in(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_in_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(ring, s[n]))
        {
            log_error("UART-in %u full", uart);
            return n;
        }
    return len;
}


unsigned uart_ring_out(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_out_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(ring, s[n]))
        {
            if (uart)
                log_error("UART-out %u full", uart);
            else
                platform_raw_msg("Debug UART ring full!");
            return n;
        }
    return len;
}


unsigned cmd_ring_out(const char* s, unsigned len)
{
    for (unsigned n = 0; n < len; n++)
        if (!ring_buf_add(&cmd_ring_out_buf, s[n]))
        {
            log_error("UART-CMD full");
            return n;
        }
    return len;

}


struct _usb_out_async_packet
{
    unsigned uart;
    bool complete;
};


static unsigned _usb_out_async(char * tmp, unsigned len, void * data)
{
    struct _usb_out_async_packet * packet = (struct _usb_out_async_packet*)data;
    unsigned r = usb_uart_send(packet->uart, tmp, len, packet->complete);

    log_debug(DEBUG_UART, "UART %u -> USB %u (%u).", packet->uart, len, r);

    return r;
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
        log_debug(DEBUG_UART, "UART %u IN %u", uart, len);

    if (uart)
    {
        struct _usb_out_async_packet packet = {.uart = uart, .complete=true};
        char * tmp = usb_out_packets[uart];

        if (len > USB_DATA_PCK_SZ)
        {
            len = USB_DATA_PCK_SZ;
            packet.complete = false;
        }

        ring_buf_consume(ring, _usb_out_async, tmp, len, &packet);
    }
    else
    {
        len = ring_buf_readline(ring, command, CMD_LINELEN);

        if (len)
            cmds_process(command, len);
    }
}


static unsigned _uart_out_dma(char * c, unsigned len, void * puart)
{
    unsigned uart = *(unsigned*)puart;

    if (uart_dma_out(uart, c, len))
        return len;
    return 0;
}


static void uart_ring_out_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_out_bufs[uart];

    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

    if (uart_is_tx_empty(uart))
    {
        if (uart)
            log_debug(DEBUG_UART, "UART %u OUT %u", uart, len);

        len = (len > DMA_DATA_PCK_SZ)?DMA_DATA_PCK_SZ:len;

        ring_buf_consume(ring, _uart_out_dma, uart_dma_buf[uart], len, &uart);
    }
}


static void cmd_ring_out_drain()
{
    unsigned len = ring_buf_get_pending(&cmd_ring_out_buf);

    if (!len)
        return;

    log_debug(DEBUG_UART, "CMD UART %u", len);

    struct _usb_out_async_packet packet = {.uart = 0, .complete=true};

    if (len > USB_DATA_PCK_SZ)
    {
        len = USB_DATA_PCK_SZ;
        packet.complete = false;
    }

    ring_buf_consume(&cmd_ring_out_buf, _usb_out_async, usb_out_packets[0], len, &packet);
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


void uart_ring_check(ring_buf_t * ring, char * name, unsigned index)
{
    log_debug(DEBUG_UART, "UART %u %s r_pos %u", index, name, ring->r_pos);
    log_debug(DEBUG_UART, "UART %u %s w_pos %u", index, name, ring->w_pos);
    log_debug(DEBUG_UART, "UART %u %s pending %u", index, name, ring_buf_get_pending(ring));

    if (ring_buf_is_full(ring))
        log_error("UART %u %s ring buffer filled.", index, name);
}


void uart_rings_check()
{
    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        ring_buf_t * in_ring  = &ring_in_bufs[n];
        ring_buf_t * out_ring = &ring_out_bufs[n];

        uart_ring_check(in_ring, "in", n);
        uart_ring_check(out_ring, "out", n);
    }

    uart_ring_check(&cmd_ring_out_buf, "CMD", 0);
}
