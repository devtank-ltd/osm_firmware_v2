#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include <osm/core/uarts.h>

#include "linux.h"
#include <osm/core/uart_rings.h>
#include <osm/core/sleep.h>
#include <osm/core/log.h>
#include "pinmap.h"
#include <osm/core/common.h>

static osm_uart_channel_t uart_channels[] = UART_CHANNELS;


void osm_linux_uart_proc(unsigned uart, char* in, unsigned len)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;

    unsigned written = osm_uart_ring_in(uart, in, len);
    if (written != len)
    {
        if (uart)
            osm_linux_error("Failed to write all uart:%u len:%u", uart, len);
        else
            osm_linux_port_debug("Failed to write all to uart:0 len:%u", len);
    }

    if (uart == CMD_UART)
    {
        for (unsigned i = 0; i < len; i++)
        {
            if (in[i] == '\n' || in[i] == '\r')
            {
                osm_sleep_debug("Waking up on command.");
                osm_sleep_exit_sleep_mode();
            }
        }
    }
    else
    {
        osm_linux_port_debug("UART:%u now has %u", uart, osm_uart_ring_in_get_len(uart));
        osm_sleep_debug("Waking up on receive data.");
        osm_sleep_exit_sleep_mode();
    }
}


void osm_uarts_setup(void)
{
}


void osm_uart_enable(unsigned uart, bool enable)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;
    uart_channels[uart].enabled = enable;
}


bool osm_uart_is_enabled(unsigned uart)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return false;
    return uart_channels[uart].enabled;
}


void osm_uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_osm_uart_parity_t parity, osm_osm_uart_stop_bits_t stop, osm_cmd_ctx_t * ctx)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return;

    if (databits < 7)
    {
        osm_cmd_ctx_error(ctx,"Invalid low UART databits, using 7");
        databits = 7;
    }
    else if (databits > 9)
    {
        osm_cmd_ctx_error(ctx,"Invalid high UART databits, using 9");
        databits = 9;
    }

    if (parity && databits == 9)
    {
        osm_cmd_ctx_error(ctx,"Invalid UART databits:9 + parity, using 9N");
        parity = osm_uart_parity_none;
    }

    osm_uart_channel_t* chan = &uart_channels[uart];
    chan->baud      = speed;
    chan->databits  = databits;
    chan->parity    = parity;
    chan->stop      = stop;

    osm_uart_debug(uart, "%u %"PRIu8"%c%s",
            (unsigned)chan->baud, chan->databits, osm_uart_parity_as_char(chan->parity), osm_uart_stop_bits_as_str(chan->stop));
}


bool osm_uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_osm_uart_parity_t * parity, osm_osm_uart_stop_bits_t * stop)
{
    if (uart >= OSM_UART_CHANNELS_COUNT)
        return false;
    osm_uart_channel_t* chan = &uart_channels[uart];
    *speed      = chan->baud;
    *databits   = chan->databits;
    *parity     = chan->parity;
    *stop       = chan->stop;
    return true;
}


bool osm_uart_resetup_str(unsigned uart, char * str, osm_cmd_ctx_t * ctx)
{
        return true;
    uint32_t         speed;
    uint8_t          databits;
    osm_osm_uart_parity_t    parity;
    osm_osm_uart_stop_bits_t stop;

    if (uart >= OSM_UART_CHANNELS_COUNT )
    {
        osm_log_error("INVALID UART GIVEN");
        return false;
    }

    if (!osm_decompose_uart_str(str, &speed, &databits, &parity, &stop))
        return false;

    osm_uart_resetup(uart, speed, databits, parity, stop, ctx);
    return true;
}


bool osm_uart_is_tx_empty(unsigned uart)
{
    return true;
}


static void _uart_blocking(unsigned uart, const char *data, int size)
{
    if (log_async_log)
    {
        if (!osm_linux_write_pty(uart, data, size))
            osm_linux_port_debug("Write failed.");
    }
    else if (uart == CMD_UART)
    {
        if (linux_has_reset)
        {
            if (!osm_linux_write_pty(uart, data, size))
                osm_linux_port_debug("Write failed.");
        }
        else if (fwrite((char*)data, 1, size, stdout) != (size_t)size)
            exit(-1);
    }
}


void osm_uart_blocking(unsigned uart, const char *data, unsigned size)
{
    _uart_blocking(uart, data, size);
}


unsigned osm_uart_dma_out(unsigned uart, char *data, unsigned size)
{
    _uart_blocking(uart, data, size);
    return size;
}
