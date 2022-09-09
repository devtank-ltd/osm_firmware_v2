#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

#include "uarts.h"

#include "linux.h"
#include "uart_rings.h"
#include "sleep.h"
#include "log.h"


#define UART_CHANNELS_LINUX                                                                     \
{                                                                                               \
    { UART_2_SPEED, UART_2_DATABITS, UART_2_PARITY, UART_2_STOP, true, 0}, /* UART 0 Debug */   \
    { UART_3_SPEED, UART_3_DATABITS, UART_3_PARITY, UART_3_STOP, true, 0}, /* UART 1 LoRa */    \
    { UART_1_SPEED, UART_1_DATABITS, UART_1_PARITY, UART_1_STOP, true, 0}, /* UART 2 HPM */     \
    { UART_4_SPEED, UART_4_DATABITS, UART_4_PARITY, UART_4_STOP, true, 0}, /* UART 3 485 */     \
}


static uart_channel_t uart_channels[] = UART_CHANNELS_LINUX;


static void _uart_proc(unsigned uart, char* in, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    uart_ring_in(uart, in, len);
    for (unsigned i = 0; i < len; i++)
    {
        if (in[i] == '\n' || in[i] == '\r')
        {
            sleep_debug("Waking up.");
            sleep_exit_sleep_mode();
        }
    }
}


static void _uart_debug_cb(char* in, unsigned len)
{
    _uart_proc(CMD_UART, in, len);
}


static void _uart_lw_cb(char* in, unsigned len)
{
    _uart_proc(LW_UART, in, len);
}


static void _uart_hpm_cb(char* in, unsigned len)
{
    _uart_proc(HPM_UART, in, len);
}


static void _uart_rs485_cb(char* in, unsigned len)
{
    _uart_proc(RS485_UART, in, len);
}


void uarts_linux_setup(void)
{
    linux_add_pty("UART_DEBUG", &uart_channels[0].fd, _uart_debug_cb);
    linux_add_pty("UART_LW", &uart_channels[1].fd, _uart_lw_cb);
    linux_add_pty("UART_HPM", &uart_channels[2].fd, _uart_hpm_cb);
    linux_add_pty("UART_RS485", &uart_channels[2].fd, _uart_rs485_cb);
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


void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop)
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
            (unsigned)chan->baud, chan->databits, uart_parity_as_char(chan->parity), uart_stop_bits_as_str(chan->stop));
}


bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, uart_parity_t * parity, uart_stop_bits_t * stop)
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
    uint32_t         speed;
    uint8_t          databits;
    uart_parity_t    parity;
    uart_stop_bits_t stop;

    if (uart >= UART_CHANNELS_COUNT )
    {
        log_error("INVALID UART GIVEN");
        return false;
    }

    if (!decompose_uart_str(str, &speed, &databits, &parity, &stop))
        return false;

    uart_resetup(uart, speed, databits, parity, stop);
    return true;
}


bool uart_is_tx_empty(unsigned uart)
{
    return true;
}


bool uart_single_out(unsigned uart, char c)
{
    printf("Unimplemented uart_single_out");
    exit(-1);
}


void uart_blocking(unsigned uart, const char *data, int size)
{
    if (!write(uart_channels[uart].fd, data, size))
        exit(-1);
}


bool uart_dma_out(unsigned uart, char *data, int size)
{
    return (write(uart_channels[uart].fd, data, size) != 0);
}
