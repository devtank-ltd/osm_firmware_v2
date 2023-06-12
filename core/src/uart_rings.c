#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "pinmap.h"
#include "uart_rings.h"
#include "ring.h"
#include "log.h"
#include "cmd.h"
#include "uarts.h"
#include "comms.h"
#include "platform.h"
#include "platform_model.h"

#include "protocol.h"


#define UART_RATE_LIMIT_MS              250


typedef char dma_uart_buf_t[DMA_DATA_PCK_SZ];

UART_BUFFERS_INIT

static ring_buf_t ring_in_bufs[UART_CHANNELS_COUNT]=UART_IN_RINGS;
static ring_buf_t ring_out_bufs[UART_CHANNELS_COUNT]=UART_OUT_RINGS;

char line_buffer[CMD_LINELEN];

static dma_uart_buf_t uart_dma_buf[UART_CHANNELS_COUNT];


bool uart_ring_out_busy(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return 0;

    ring_buf_t * ring = &ring_out_bufs[uart];
    return ring_buf_get_pending(ring);
}


bool uart_rings_out_busy(void)
{
    for (unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        if (uart_ring_out_busy(n))
            return true;
    }
    return false;
}


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

    if (ring->size == 1)
    {
        if (!uart_is_tx_empty(uart))
        {
            log_error("UART %u already DMAing without ring.", uart);
            return 0;
        }

        if (len > DMA_DATA_PCK_SZ)
        {
            log_error("String too big for UART %u DMA without ring.", uart);
            return 0;
        }

        char *dma_mem = uart_dma_buf[uart];

        memcpy(dma_mem, s, len);

        uart_dma_out(uart, dma_mem, len);

        return len;
    }

    if (uart) // Add debug messages to debug ring buffer is a loop.
        log_debug(DEBUG_UART(uart), "UART %u out < %u", uart, len);

    static uint32_t last_sent[UART_CHANNELS_COUNT] = {0};

    for (unsigned n = 0; n < len; n++)
    {
        if (!ring_buf_add(ring, s[n]) &&
            since_boot_delta(get_since_boot_ms(), last_sent[uart]) > UART_RATE_LIMIT_MS)
        {
            last_sent[uart] = get_since_boot_ms();
            if (uart)
                log_error("UART-out %u full", uart);
            else
                platform_raw_msg("Debug UART ring full!");
            return n;
        }
    }
    return len;
}


void uart_ring_in_drain(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    ring_buf_t * ring = &ring_in_bufs[uart];

    unsigned len = ring_buf_get_pending(ring);

    if (uart && len)
        log_debug(DEBUG_UART(uart), "UART %u IN %u", uart, len);

    if (model_uart_ring_done_in_process(uart, ring))
        return;

    if (!len)
        return;

    if (uart == CMD_UART)
    {
        len = ring_buf_readline(ring, line_buffer, CMD_LINELEN);

        if (len)
            cmds_process(line_buffer, len);
    }
    else if (uart == COMMS_UART)
    {
        len = ring_buf_readline(ring, line_buffer, CMD_LINELEN);
        if (len)
        {
            comms_debug(" >> %s", line_buffer);
            protocol_process(line_buffer);
        }
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

    if(!model_uart_ring_do_out_drain(uart, ring))
        return;

    unsigned len = ring_buf_get_pending(ring);

    if (!len)
        return;

    if (uart_is_tx_empty(uart))
    {
        if (uart)
            log_debug(DEBUG_UART(uart), "UART %u OUT > %u", uart, len);

        len = (len > DMA_DATA_PCK_SZ)?DMA_DATA_PCK_SZ:len;

        ring_buf_consume(ring, _uart_out_dma, uart_dma_buf[uart], len, &uart);
    }
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
}


static void uart_ring_check(ring_buf_t * ring, char * name, unsigned index)
{
    log_debug(DEBUG_UART(index), "UART %u %s r_pos %u", index, name, ring->r_pos);
    log_debug(DEBUG_UART(index), "UART %u %s w_pos %u", index, name, ring->w_pos);
    log_debug(DEBUG_UART(index), "UART %u %s pending %u", index, name, ring_buf_get_pending(ring));

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
}


void uart_rings_init(void)
{
}


static void _uart_rings_wipe(ring_buf_t * ring)
{
    memset((void*)ring->buf, 0, sizeof(ring->size));
    ring->r_pos = 0;
    ring->w_pos = 0;
}


void uart_rings_in_wipe(unsigned uart)
{
    if (uart < UART_CHANNELS_COUNT)
    {
        _uart_rings_wipe(&ring_in_bufs[uart]);
    }
}


void uart_rings_out_wipe(unsigned uart)
{
    if (uart < UART_CHANNELS_COUNT)
    {
        _uart_rings_wipe(&ring_out_bufs[uart]);
    }
}
