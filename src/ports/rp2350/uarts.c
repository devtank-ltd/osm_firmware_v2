#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>

#include "hardware/uart.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include <osm/core/cmd.h>
#include <osm/core/log.h>
#include <osm/core/ring.h>
#include <osm/core/uart_rings.h>
#include <osm/core/uarts.h>
#include <osm/core/sleep.h>
#include <osm/core/common.h>
#include "pinmap.h"
#include "platform_model.h"


static uart_channel_t _uarts_channels[UART_CHANNELS_COUNT] = UART_CHANNELS;
static int _uarts_dma_channels[UART_CHANNELS_COUNT] = {-1};


static uart_parity_t _uart_get_parity(osm_uart_parity_t parity)
{
    switch(parity)
    {
        case uart_parity_odd  : return UART_PARITY_ODD;
        case uart_parity_even : return UART_PARITY_EVEN;
        default : return UART_PARITY_NONE;
    }
}

static uint32_t _uart_get_stop(osm_uart_stop_bits_t stop)
{
    switch(stop)
    {
        case uart_stop_bits_2  : return 2;
        default : return 1;
    }
}


static uart_inst_t* _uart_up(const uart_channel_t * channel)
{
    uart_inst_t* uart_inst = uart_get_instance(channel->uart);

    uart_init(uart_inst, channel->baud);

    uint tx_pin = channel->tx_pin;
    uint rx_pin = channel->rx_pin;

    gpio_set_function(tx_pin, UART_FUNCSEL_NUM(uart_inst, tx_pin));
    gpio_set_function(rx_pin, UART_FUNCSEL_NUM(uart_inst, rx_pin));

    uint32_t databits = channel->databits;
    uart_parity_t parity = _uart_get_parity(channel->parity);

    uint32_t stop_bits = _uart_get_stop(channel->stop);
    uart_set_format(uart_inst, databits, stop_bits, parity);
    return uart_inst;
}



static void _uart_process(void)
{
    for (unsigned uart = 0; uart < UART_CHANNELS_COUNT; uart++)
    {
        uart_channel_t* channel = &_uarts_channels[uart];
        uart_inst_t* uart_inst = uart_get_instance(channel->uart);
        if (uart_is_readable(uart_inst))
        {
            char c = uart_getc(uart_inst);
            uart_ring_in(uart, &c, 1);
        }
    }
}


static void uart_setup(unsigned uart)
{
    uart_channel_t* channel = &_uarts_channels[uart];
    uart_inst_t* uart_inst = _uart_up(channel);

    uart_set_hw_flow(uart_inst, false, false);
    uart_set_fifo_enabled(uart_inst, false);

    uint dreq = uart_get_dreq_num(uart_inst, true);
    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config dma_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);

    channel_config_set_dreq(&dma_config, dreq);
    dma_channel_set_config(dma_channel, &dma_config, false);
    uart_hw_t* uart_hw = uart_get_hw(uart_inst);
    dma_channel_set_write_addr(dma_channel, &uart_hw->dr, false);

    irq_num_t uart_irq = UART_IRQ_NUM(uart_inst);
    irq_set_exclusive_handler(uart_irq, _uart_process);
    irq_set_enabled(uart_irq, true);
    uart_set_irqs_enabled(uart_inst, true, false);

    _uarts_dma_channels[uart] = dma_channel;

    channel->enabled = 1;
}


void uart_enable(unsigned uart, bool enable)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    uart_debug(uart, "%s", (enable)?"Enable":"Disable");

    uart_channel_t * channel = &_uarts_channels[uart];

    if (!enable)
    {
        uart_inst_t* uart_inst = uart_get_instance(channel->uart);
        uart_set_irqs_enabled(uart_inst, false, false);
        int dma_channel = _uarts_dma_channels[uart];
        if (0 <= dma_channel)
        {
            dma_channel_abort(dma_channel);
            dma_channel_cleanup(dma_channel);
            dma_channel_unclaim(dma_channel);
            _uarts_dma_channels[uart] = -1;
        }
        uart_deinit(uart_inst);
        channel->enabled = 0;
    }
    else uart_setup(uart);
}


void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop, cmd_ctx_t * ctx)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    uart_channel_t * channel = &_uarts_channels[uart];

    if (databits < 7)
    {
        cmd_ctx_error(ctx, "Invalid low UART databits, using 7");
        databits = 7;
    }
    else if (databits > 9)
    {
        cmd_ctx_error(ctx, "Invalid high UART databits, using 9");
        databits = 9;
    }

    if (parity && databits == 9)
    {
        cmd_ctx_error(ctx, "Invalid UART databits:9 + parity, using 9N");
        parity = uart_parity_none;
    }

    channel->baud = speed;
    channel->databits = databits;
    channel->parity = parity;
    channel->stop = stop;

    uart_inst_t* uart_inst = uart_get_instance(channel->uart);
    uart_set_irqs_enabled(uart_inst, false, false);
    uart_deinit(uart_inst);
    _uart_up(channel);
    uart_set_irqs_enabled(uart_inst, true, false);

    uart_debug(uart, "%u %"PRIu8"%c%s",
            (unsigned)channel->baud, channel->databits, osm_uart_parity_as_char(channel->parity), osm_uart_stop_bits_as_str(channel->stop));
}


extern bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_uart_parity_t * parity, osm_uart_stop_bits_t * stop)
{
    if (uart >= UART_CHANNELS_COUNT )
        return false;

    const uart_channel_t * channel = &_uarts_channels[uart];

    if (speed)
        *speed = channel->baud;

    if (databits)
        *databits = channel->databits;

    if (parity)
        *parity = channel->parity;

    if (stop)
        *stop = channel->stop;

    return true;
}


bool uart_resetup_str(unsigned uart, char * str, cmd_ctx_t * ctx)
{
    uint32_t         speed;
    uint8_t          databits;
    osm_uart_parity_t    parity;
    osm_uart_stop_bits_t stop;

    if (uart >= UART_CHANNELS_COUNT )
    {
        cmd_ctx_error(ctx, "INVALID UART GIVEN");
        return false;
    }

    if (!osm_decompose_uart_str(str, &speed, &databits, &parity, &stop))
        return false;

    uart_resetup(uart, speed, databits, parity, stop, ctx);
    return true;
}


void uarts_setup(void)
{
    model_uarts_setup();

    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_setup(n);
}

bool uart_is_tx_empty(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    int dma_channel = _uarts_dma_channels[uart];
    if (0 > dma_channel || dma_channel_is_busy(dma_channel))
        return false;

    uart_inst_t* uart_inst = uart_get_instance(_uarts_channels[uart].uart);
    /* As we aren't using any FIFO, the TX will be empty if the UART is
     * writable */
    return uart_is_writable(uart_inst);
}


void uart_blocking(unsigned uart, const char *data, unsigned size)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    uart_inst_t* uart_inst = uart_get_instance(_uarts_channels[uart].uart);

    while(size--)
        uart_putc_raw(uart_inst, *data++);
}


unsigned uart_dma_out(unsigned uart, char *data, unsigned size)
{
    if (uart >= UART_CHANNELS_COUNT)
    {
        return 0;
    }

    const uart_channel_t * channel = &_uarts_channels[uart];

    if (!channel->enabled)
    {
        return size; /* Drop the data */
    }

    if (!uart_is_tx_empty(uart))
    {
        return 0;
    }

    uart_inst_t* uart_inst = uart_get_instance(channel->uart);
    if (size == 1)
    {
        if (uart)
            uart_debug(uart, "single out.");
        uart_putc_raw(uart_inst, *data);
        return true;
    }

    if (uart)
    {
        uart_debug(uart, "%u out on DMA", size);
    }

    int dma_channel = _uarts_dma_channels[uart];
    if (0 > dma_channel)
    {
        /* No DMA assigned */
        return 0;
    }
    dma_channel_transfer_from_buffer_now(dma_channel, data, size);
    return size;
}
