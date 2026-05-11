#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "pinmap.h"
#include <osm/core/uart_rings.h>
#include <osm/core/ring.h>
#include <osm/core/log.h>
#include <osm/core/cmd.h>
#include <osm/core/uarts.h>
#include <osm/core/common.h>
#include <osm/core/platform.h>
#include "platform_model.h"

#include <osm/protocols/protocol.h>


#define UART_RATE_LIMIT_MS              250


typedef char dma_uart_buf_t[OSM_DMA_DATA_PCK_SZ];

OSM_UART_BUFFERS_INIT

static osm_ring_buf_t ring_in_bufs[OSM_UART_CHANNELS_COUNT]=OSM_UART_IN_RINGS;
static osm_ring_buf_t ring_out_bufs[OSM_UART_CHANNELS_COUNT]=OSM_UART_OUT_RINGS;

char line_buffer[CMD_LINELEN];

static dma_uart_buf_t uart_dma_buf[OSM_UART_CHANNELS_COUNT];

static void _uart_cmd_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_cmd_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap);
static void _uart_cmd_flush(osm_cmd_ctx_t * ctx);

osm_cmd_ctx_t uart_cmd_ctx = {.output_cb = _uart_cmd_out,
                          .error_cb = _uart_cmd_error,
                          .flush_cb = _uart_cmd_flush };


static void _uart_cmd_out(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_outv(fmt, ap);
}


static void _uart_cmd_error(osm_cmd_ctx_t * ctx, const char * fmt, va_list ap)
{
    osm_log_errorv(fmt, ap);
}


static void _uart_cmd_flush(osm_cmd_ctx_t * ctx)
{
    osm_log_out_drain(1000);
}



bool osm_uart_ring_out_busy(unsigned uart)
{
    return osm_uart_ring_out_get_len(uart) > 0;
}


bool osm_uart_rings_out_busy(void)
{
    for (unsigned n = 0; n < OSM_UART_CHANNELS_COUNT; n++)
    {
        if (osm_uart_ring_out_busy(n))
            return true;
    }
    return false;
}


unsigned osm_uart_ring_in(unsigned uart, const char* s, unsigned len)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return 0;

    osm_ring_buf_t * ring = &ring_in_bufs[uart];

    for (unsigned n = 0; n < len; n++)
        if (!osm_ring_buf_add(ring, s[n]))
        {
            osm_log_error("UART-in %u full", uart);
            return n;
        }
    return len;
}


unsigned osm_uart_ring_out(unsigned uart, const char* s, unsigned len)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return 0;

    osm_ring_buf_t * ring = &ring_out_bufs[uart];

    if (ring->size == 1)
    {
        if (!osm_uart_is_tx_empty(uart))
        {
            osm_log_error("UART %u already DMAing without ring.", uart);
            return 0;
        }

        if (len > OSM_DMA_DATA_PCK_SZ)
        {
            osm_log_error("String too big for UART %u DMA without ring.", uart);
            return 0;
        }

        char *dma_mem = uart_dma_buf[uart];

        memcpy(dma_mem, s, len);

        return osm_uart_dma_out(uart, dma_mem, len);
    }

    if (uart) // Add debug messages to debug ring buffer is a loop.
        osm_log_debug(DEBUG_UART(uart), "UART %u out < %u", uart, len);

    static uint32_t last_sent[OSM_UART_CHANNELS_COUNT] = {0};

    for (unsigned n = 0; n < len; n++)
    {
        if (!osm_ring_buf_add(ring, s[n]) &&
            osm_since_boot_delta(osm_get_since_boot_ms(), last_sent[uart]) > UART_RATE_LIMIT_MS)
        {
            last_sent[uart] = osm_get_since_boot_ms();
            if (uart)
                osm_log_error("UART-out %u full", uart);
            else
                osm_platform_raw_msg("Debug UART ring full!");
            return n;
        }
    }
    return len;
}


unsigned osm_uart_ring_in_get_len(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return 0;
    return osm_ring_buf_get_pending(&ring_in_bufs[uart]);
}


unsigned osm_uart_ring_out_get_len(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return 0;
    return osm_ring_buf_get_pending(&ring_out_bufs[uart]);
}


void osm_uart_ring_in_drain(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;

    osm_ring_buf_t * ring = &ring_in_bufs[uart];

    unsigned len = osm_ring_buf_get_pending(ring);

    if (uart && len)
        osm_log_debug(DEBUG_UART(uart), "UART %u IN %u", uart, len);

    if (osm_model_uart_ring_done_in_process(uart, ring))
        return;

    if (!len)
        return;

    if (uart == CMD_UART)
    {
        len = osm_ring_buf_readline(ring, line_buffer, CMD_LINELEN);

        if (len)
            osm_cmds_process(line_buffer, len, &uart_cmd_ctx);
    }
#ifdef COMMS_UART
    else if (uart == COMMS_UART)
    {
        len = osm_ring_buf_readline(ring, line_buffer, CMD_LINELEN);
        if (len)
        {
            osm_comms_debug(" >> %s", line_buffer);
            osm_protocol_process(line_buffer);
        }
    }
#endif
}


static unsigned _uart_out_dma(char * c, unsigned len, void * puart)
{
    unsigned uart = *(unsigned*)puart;

    return osm_uart_dma_out(uart, c, len);
}


static void uart_ring_out_drain(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;

    osm_ring_buf_t * ring = &ring_out_bufs[uart];

    if(!osm_model_uart_ring_do_out_drain(uart, ring))
        return;

    unsigned len = osm_ring_buf_get_pending(ring);

    if (!len)
        return;

    if (osm_uart_is_tx_empty(uart))
    {
        if (uart)
            osm_log_debug(DEBUG_UART(uart), "UART %u OUT > %u", uart, len);

        len = (len > OSM_DMA_DATA_PCK_SZ)?OSM_DMA_DATA_PCK_SZ:len;

        osm_ring_buf_consume(ring, _uart_out_dma, uart_dma_buf[uart], len, &uart);
    }
}


void osm_uart_rings_in_drain()
{
    for(unsigned n = 0; n < OSM_UART_CHANNELS_COUNT; n++)
        osm_uart_ring_in_drain(n);
}


void osm_uart_rings_out_drain()
{
    for(unsigned n = 0; n < OSM_UART_CHANNELS_COUNT; n++)
        uart_ring_out_drain(n);
}


void osm_uart_rings_drain_all_out(void)
{
    while (osm_uart_rings_out_busy())
    {
        osm_uart_rings_out_drain();
    }
}


static void uart_ring_check(osm_ring_buf_t * ring, const char * name, unsigned index)
{
    osm_log_debug(DEBUG_UART(index), "UART %u %s r_pos %u", index, name, ring->r_pos);
    osm_log_debug(DEBUG_UART(index), "UART %u %s w_pos %u", index, name, ring->w_pos);
    osm_log_debug(DEBUG_UART(index), "UART %u %s pending %u", index, name, osm_ring_buf_get_pending(ring));

    if (osm_ring_buf_is_full(ring))
        osm_log_error("UART %u %s ring buffer filled.", index, name);
}


void osm_uart_rings_check()
{
    for(unsigned n = 0; n < OSM_UART_CHANNELS_COUNT; n++)
    {
        osm_ring_buf_t * in_ring  = &ring_in_bufs[n];
        osm_ring_buf_t * out_ring = &ring_out_bufs[n];

        uart_ring_check(in_ring, "in", n);
        uart_ring_check(out_ring, "out", n);
    }
}


void osm_uart_rings_init(void)
{
}


static void _uart_rings_wipe(osm_ring_buf_t * ring)
{
    memset((void*)ring->buf, 0, sizeof(ring->size));
    ring->r_pos = 0;
    ring->w_pos = 0;
}


void osm_uart_rings_in_wipe(unsigned uart)
{
    if (uart < OSM_UART_CHANNELS_COUNT)
    {
        _uart_rings_wipe(&ring_in_bufs[uart]);
    }
}


void osm_uart_rings_out_wipe(unsigned uart)
{
    if (uart < OSM_UART_CHANNELS_COUNT)
    {
        _uart_rings_wipe(&ring_out_bufs[uart]);
    }
}
