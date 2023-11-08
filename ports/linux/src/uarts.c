#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include "uarts.h"

#include "linux.h"
#include "uart_rings.h"
#include "sleep.h"
#include "log.h"
#include "pinmap.h"

static uart_channel_t uart_channels[] = UART_CHANNELS;


void linux_uart_proc(unsigned uart, char* in, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    unsigned written = uart_ring_in(uart, in, len);
    if (written != len)
        linux_error("Failed to write all uart:%u len:%u", uart, len);

    if (uart == CMD_UART)
    {
        for (unsigned i = 0; i < len; i++)
        {
            if (in[i] == '\n' || in[i] == '\r')
            {
                sleep_debug("Waking up on command.");
                sleep_exit_sleep_mode();
            }
        }
    }
    else
    {
        linux_port_debug("UART:%u now has %u", uart, uart_ring_in_get_len(uart));
        sleep_debug("Waking up on receive data.");
        sleep_exit_sleep_mode();
    }
}


void uarts_setup(void)
{
}


void uart_enable(unsigned uart, bool enable)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;
    uart_channels[uart].enabled = enable;
}


bool uart_is_enabled(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;
    return uart_channels[uart].enabled;
}


void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    if (databits < 7)
    {
        log_error("Invalid low UART databits, using 7");
        databits = 7;
    }
    else if (databits > 9)
    {
        log_error("Invalid high UART databits, using 9");
        databits = 9;
    }

    if (parity && databits == 9)
    {
        log_error("Invalid UART databits:9 + parity, using 9N");
        parity = uart_parity_none;
    }

    uart_channel_t* chan = &uart_channels[uart];
    chan->baud      = speed;
    chan->databits  = databits;
    chan->parity    = parity;
    chan->stop      = stop;

    uart_debug(uart, "%u %"PRIu8"%c%s",
            (unsigned)chan->baud, chan->databits, osm_uart_parity_as_char(chan->parity), osm_uart_stop_bits_as_str(chan->stop));
}


bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_uart_parity_t * parity, osm_uart_stop_bits_t * stop)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;
    uart_channel_t* chan = &uart_channels[uart];
    *speed      = chan->baud;
    *databits   = chan->databits;
    *parity     = chan->parity;
    *stop       = chan->stop;
    return true;
}


bool uart_resetup_str(unsigned uart, char * str)
{
        return true;
    uint32_t         speed;
    uint8_t          databits;
    osm_uart_parity_t    parity;
    osm_uart_stop_bits_t stop;

    if (uart >= UART_CHANNELS_COUNT )
    {
        log_error("INVALID UART GIVEN");
        return false;
    }

    if (!osm_decompose_uart_str(str, &speed, &databits, &parity, &stop))
        return false;

    uart_resetup(uart, speed, databits, parity, stop);
    return true;
}


bool uart_is_tx_empty(unsigned uart)
{
    return true;
}


static void _uart_blocking(unsigned uart, const char *data, int size)
{
    if (log_async_log)
    {
        if (!linux_write_pty(uart, data, size))
            linux_port_debug("Write failed.");
    }
    else if (uart == CMD_UART)
    {
        if (linux_has_reset)
        {
            if (!linux_write_pty(uart, data, size))
                linux_port_debug("Write failed.");
        }
        else if (fwrite((char*)data, 1, size, stdout) != (size_t)size)
            exit(-1);
    }
}


void uart_blocking(unsigned uart, const char *data, unsigned size)
{
    _uart_blocking(uart, data, size);
}


unsigned uart_dma_out(unsigned uart, char *data, unsigned size)
{
    _uart_blocking(uart, data, size);
    return size;
}
