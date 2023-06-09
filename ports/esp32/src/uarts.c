#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>

#include <driver/uart.h>

#include "cmd.h"
#include "log.h"
#include "pinmap.h"
#include "ring.h"
#include "uart_rings.h"
#include "uarts.h"
#include "sleep.h"
#include "platform_model.h"


static uart_channel_t uart_channels[UART_CHANNELS_COUNT] = UART_CHANNELS;


static void uart_up(const uart_channel_t * channel)
{
    uart_param_config(channel->uart, &channel->uart_config);
}


static void uart_setup(uart_channel_t * channel)
{
    const int uart_buffer_size = 2 * 1024;

    uart_set_pin(channel->uart, channel->tx_pin, channel->rx_pin, -1, -1);
    uart_driver_install(channel->uart, uart_buffer_size, uart_buffer_size, 0, NULL, 0);

    uart_set_mode( channel->usart, UART_MODE_UART);
    uart_up(channel);

    channel->enabled = 1;
}


void uart_enable(unsigned uart, bool enable)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    uart_debug(uart, "%s", (enable)?"Enable":"Disable");

    uart_channel_t * channel = &uart_channels[uart];

    if (!enable)
    {
        uart_driver_delete(channel->uart);
        channel->enabled = 0;
    }
    else uart_setup(channel);
}


bool uart_is_enabled(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    return (uart_channels[uart].enabled)?true:false;
}

static osm_uart_parity_t _osm_uart_parity_get(uart_parity_t parity)
{
    switch(parity)
    {
        case UART_PARITY_EVEN: return uart_parity_even;
        case UART_PARITY_ODD: return uart_parity_odd;
        default: break;
    }

    return uart_parity_none;
}


static osm_uart_stop_bits_t _osm_uart_stop_bits_get(uart_stop_bits_t stop)
{
    switch(stop)
    {
        case UART_STOP_BITS_2: return uart_stop_bits_2;
        case UART_STOP_BITS_1_5: return uart_stop_bits_1_5;
        default: break;
    }

    return uart_stop_bits_1;
}


void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, osm_uart_parity_t parity, osm_uart_stop_bits_t stop)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    uart_channel_t * channel = &uart_channels[uart];

    if (databits < 5)
    {
        log_error("Invalid low UART databits, using 7");
        databits = 5;
    }
    else if (databits > 8)
    {
        log_error("Invalid high UART databits, using 9");
        databits = 8;
    }    
    channel->baud = speed;
    channel->databits = (uart_word_length_t)(databits - 5);

    switch(parity)
    {
        case uart_parity_even:
            channel->parity = UART_PARITY_EVEN;
            break;
        case uart_parity_odd:
            channel->parity = UART_PARITY_ODD
            break;
        default: 
            channel->parity = UART_PARITY_DISABLE;
            break;
    }

    switch(stop)
    {
        case uart_stop_bits_2:
            channel->stop = UART_STOP_BITS_2;
            break;
        case uart_stop_bits_1_5:
            channel->stop = UART_STOP_BITS_1_5;
            break;
        default:
            channel->stop = UART_STOP_BITS_1;
            break;
    }

    uart_up(channel);

    uart_debug(uart, "%u %"PRIu8"%c%s",
            (unsigned)channel->baud, databits, osm_uart_parity_as_char(_osm_uart_parity_get(parity)), osm_uart_stop_bits_as_str(_osm_uart_stop_bits_get(channel->stop)));
}


bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, osm_uart_parity_t * parity, osm_uart_stop_bits_t * stop)
{
    if (uart >= UART_CHANNELS_COUNT )
        return false;

    const uart_channel_t * channel = &uart_channels[uart];

    if (speed)
        *speed = channel->baud;

    if (databits)
        *databits = ((uint8_t)channel->databits) + 5;

    if (parity)
        *parity = _osm_uart_parity_get(channel->parity);

    if (stop)
        *stop = _osm_uart_stop_bits_get(channel->stop);

    return true;
}


bool uart_resetup_str(unsigned uart, char * str)
{
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


static bool uart_getc(uint32_t uart, char* c)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;
    uart_channel_t * channel = &uart_channels[uart];
    int r = uart_read_bytes(channel->uart, c, 1, 0);
    return (r == 1);
}



static void process_serial(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    uart_channel_t * channel = &uart_channels[uart];

    if (!channel->enabled)
        return;

    char c;

    if (!uart_getc(channel->usart, &c))
        return;

    uart_ring_in(uart, &c, 1);
    if (c == '\n' || c == '\r')
    {
        sleep_debug("Waking up.");
        sleep_exit_sleep_mode();
    }
}


void uarts_setup(void)
{
    model_uarts_setup();

    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_setup(&uart_channels[n]);
}

bool uart_is_tx_empty(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    uart = uart_channels[uart].usart;

    return ((USART_ISR(uart) & USART_ISR_TXE));
}


void uart_blocking(unsigned uart, const char *data, int size)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    const uart_channel_t * channel = &uart_channels[uart];

    uart = uart_channels[uart].usart;

    while(size)
    {
        esp_err_t err = uart_wait_tx_done(uart, 200);
        if(err == ESP_OK)
        {
            int sent = uart_tx_chars(uart, data, size);
            if (sent >= 0)
            {
                size -= sent;
                str  += sent;
            }
            else if (sent < 0)
            {
                ERROR_PRINTF("Error writing to UART");
            }
        }
        else
        {
            ERROR_PRINTF("Trouble %s writing to the LoRa UART.", esp_err_to_name(err));
        }
    }
}


bool uart_dma_out(unsigned uart, char *data, int size)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    if (uart_doing_dma[uart])
        return false;

    const uart_channel_t * channel = &uart_channels[uart];

    if (!channel->enabled)
        return true; /* Drop the data */

    uart = uart_channels[uart].usart;

    int sent = uart_tx_chars(uart, data, size);

    if (!(USART_ISR(channel->usart) & USART_ISR_TXE))
        return false;

    if (size == 1)
    {
        if (uart)
            uart_debug(uart, "single out.");
        usart_send(channel->usart, *data);
        return true;
    }

    if (uart)
        uart_debug(uart, "%u out on DMA channel %u", size, channel->dma_channel);

    uart_doing_dma[uart] = true;

    dma_channel_reset(channel->dma_unit, channel->dma_channel);

    dma_set_peripheral_address(channel->dma_unit, channel->dma_channel, channel->dma_addr);
    dma_set_memory_address(channel->dma_unit, channel->dma_channel, (uint32_t)data);
    dma_set_number_of_data(channel->dma_unit, channel->dma_channel, size);
    dma_set_read_from_memory(channel->dma_unit, channel->dma_channel);
    dma_enable_memory_increment_mode(channel->dma_unit, channel->dma_channel);
    dma_set_peripheral_size(channel->dma_unit, channel->dma_channel, DMA_CCR_PSIZE_8BIT);
    dma_set_memory_size(channel->dma_unit, channel->dma_channel, DMA_CCR_MSIZE_8BIT);
    dma_set_priority(channel->dma_unit, channel->dma_channel, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(channel->dma_unit, channel->dma_channel);

    dma_enable_channel(channel->dma_unit, channel->dma_channel);

    usart_enable_tx_dma(channel->usart);

    return true;
}


// cppcheck-suppress unusedFunction ; System handler
void uart0_in_isr(void)
{
    process_serial(0);
}

// cppcheck-suppress unusedFunction ; System handler
void uart1_in_isr(void)
{
    process_serial(1);
}

// cppcheck-suppress unusedFunction ; System handler
void uart2_in_isr(void)
{
    process_serial(2);
}

// cppcheck-suppress unusedFunction ; System handler
void uart3_in_isr(void)
{
    process_serial(3);
}

