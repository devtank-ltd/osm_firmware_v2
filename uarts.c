#include <stdlib.h>
#include <inttypes.h>

#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/cm3/nvic.h>

#include "cmd.h"
#include "log.h"
#include "pinmap.h"
#include "ring.h"
#include "uart_rings.h"
#include "uarts.h"


static uart_channel_t uart_channels[] = UART_CHANNELS;

static volatile bool uart_doing_dma[UART_CHANNELS_COUNT] = {0};

#define UART_2_ISR  usart2_isr



static uint32_t _uart_get_parity(uart_parity_t parity)
{
    switch(parity)
    {
        case uart_parity_odd  : return USART_PARITY_ODD;
        case uart_parity_even : return USART_PARITY_EVEN;
        default : return USART_PARITY_NONE;
    }
}

static uint32_t _uart_get_stop(uart_stop_bits_t stop)
{
    switch(stop)
    {
        case uart_stop_bits_1_5: return USART_STOPBITS_1_5;
        case uart_stop_bits_2  : return USART_STOPBITS_2;
        default : return USART_CR2_STOPBITS_1;
    }
}


static void uart_up(const uart_channel_t * channel)
{
    uint32_t parity = _uart_get_parity(channel->parity);

    uint32_t usart = channel->usart;

    usart_set_baudrate( usart, channel->baud );

    uint8_t databits = channel->databits;
    if (parity != USART_PARITY_NONE)
        databits++;

    /*
     * Bits M1 and M0 make 2 bits for this.
     *
     * OpenCM3's usart_set_databits doesn't set both
     *
     */

    switch(databits)
    {
        case 7:
            // 7-bit character length: M[1:0] = 10
            USART_CR1(usart) |=  USART_CR1_M0;
            USART_CR1(usart) &= ~USART_CR1_M1;
            break;
        default :
            // Should happen, but revert to 8N when it does
            parity = USART_PARITY_NONE;
             // fall through
        case 8:
            // 8-bit character length: M[1:0] = 00
            USART_CR1(usart) &= ~USART_CR1_M0;
            USART_CR1(usart) &= ~USART_CR1_M1;
            break;
        case 9:
            // 9-bit character length: M[1:0] = 01
            USART_CR1(usart) &= ~USART_CR1_M0;
            USART_CR1(usart) |=  USART_CR1_M1;
            break;
    }

    usart_set_stopbits( usart, _uart_get_stop(channel->stop) );
    usart_set_parity( usart, parity);
}


static void uart_setup(uart_channel_t * channel)
{
    rcc_periph_clock_enable(PORT_TO_RCC(channel->gpioport));
    rcc_periph_clock_enable(channel->uart_clk);

    gpio_mode_setup( channel->gpioport, GPIO_MODE_AF, GPIO_PUPD_NONE, channel->pins );
    gpio_set_af( channel->gpioport, channel->alt_func_num, channel->pins );

    usart_set_mode( channel->usart, USART_MODE_TX_RX );
    usart_set_flow_control( channel->usart, USART_FLOWCONTROL_NONE );
    uart_up(channel);

    nvic_set_priority(channel->irqn, channel->priority);
    nvic_enable_irq(channel->irqn);
    usart_enable(channel->usart);
    usart_enable_rx_interrupt(channel->usart);

    if (channel->dma_irqn)
    {
        nvic_set_priority(channel->dma_irqn, channel->priority);
        nvic_enable_irq(channel->dma_irqn);
        dma_set_channel_request(DMA1, channel->dma_channel, 2);
    }
    channel->enabled = 1;
}


void uart_enable(unsigned uart, bool enable)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    log_debug(DEBUG_UART, "UART %u : %s", uart, (enable)?"Enable":"Disable");

    uart_channel_t * channel = &uart_channels[uart];

    if (!enable)
    {
        usart_disable_rx_interrupt(channel->usart);
        usart_disable(channel->usart);
        rcc_periph_clock_disable(channel->uart_clk);
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


void uart_resetup(unsigned uart, unsigned speed, uint8_t databits, uart_parity_t parity, uart_stop_bits_t stop)
{
    if (uart >= UART_CHANNELS_COUNT || !uart)
        return;

    uart_channel_t * channel = &uart_channels[uart];

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

    channel->baud = speed;
    channel->databits = databits;
    channel->parity = parity;
    channel->stop = stop;

    uart_up(channel);

    log_debug(DEBUG_UART, "UART %u : %u %"PRIu8"%c%"PRIu8, uart,
            (unsigned)channel->baud, channel->databits, uart_parity_as_char(channel->parity), uart_stop_bits_as_int(channel->stop));
}


extern bool uart_get_setup(unsigned uart, unsigned * speed, uint8_t * databits, uart_parity_t * parity, uart_stop_bits_t * stop)
{
    if (uart >= UART_CHANNELS_COUNT )
        return false;

    const uart_channel_t * channel = &uart_channels[uart];

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


static bool uart_getc(uint32_t uart, char* c)
{
    uint32_t flags = USART_ISR(uart);

    if (!(flags & USART_ISR_RXNE))
    {
        USART_ICR(uart) = flags;
        return false;
    }

    *c = usart_recv(uart);

    return true;
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
}


void usart2_isr(void)
{
    process_serial(0);
}

void uarts_setup(void)
{
    rcc_periph_clock_enable(RCC_DMA1);

    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_setup(&uart_channels[n]);
}

bool uart_is_tx_empty(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    if (uart_doing_dma[uart])
        return false;

    uart = uart_channels[uart].usart;

    return ((USART_ISR(uart) & USART_ISR_TXE));
}


bool uart_single_out(unsigned uart, char c)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    const uart_channel_t * channel = &uart_channels[uart];

    if (!(USART_ISR(channel->usart) & USART_ISR_TXE))
        return false;

    usart_send(channel->usart, c);

    return true;
}


void uart_blocking(unsigned uart, const char *data, int size)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    const uart_channel_t * channel = &uart_channels[uart];

    while(size--)
        usart_send_blocking(channel->usart, *data++);
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

    if (!(USART_ISR(channel->usart) & USART_ISR_TXE))
        return false;

    if (size == 1)
    {
        if (uart)
            log_debug(DEBUG_UART, "UART %u single out.", uart);
        usart_send(channel->usart, *data);
        return true;
    }

    if (uart)
        log_debug(DEBUG_UART, "UART %u %u out on DMA channel %u", uart, size, channel->dma_channel);

    uart_doing_dma[uart] = true;

    dma_channel_reset(DMA1, channel->dma_channel);

    dma_set_peripheral_address(DMA1, channel->dma_channel, channel->dma_addr);
    dma_set_memory_address(DMA1, channel->dma_channel, (uint32_t)data);
    dma_set_number_of_data(DMA1, channel->dma_channel, size);
    dma_set_read_from_memory(DMA1, channel->dma_channel);
    dma_enable_memory_increment_mode(DMA1, channel->dma_channel);
    dma_set_peripheral_size(DMA1, channel->dma_channel, DMA_CCR_PSIZE_8BIT);
    dma_set_memory_size(DMA1, channel->dma_channel, DMA_CCR_MSIZE_8BIT);
    dma_set_priority(DMA1, channel->dma_channel, DMA_CCR_PL_LOW);

    dma_enable_transfer_complete_interrupt(DMA1, channel->dma_channel);

    dma_enable_channel(DMA1, channel->dma_channel);

    usart_enable_tx_dma(channel->usart);

    return true;
}


static void process_complete_dma(void)
{
    unsigned found = 0;

    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
    {
        const uart_channel_t * channel = &uart_channels[n];

        if ((DMA1_ISR & DMA_ISR_TCIF(channel->dma_channel)) != 0)
        {
            DMA1_IFCR |= DMA_IFCR_CTCIF(channel->dma_channel);

            uart_doing_dma[n] = false;

            dma_disable_transfer_complete_interrupt(DMA1, channel->dma_channel);

            usart_disable_tx_dma(channel->usart);

            dma_disable_channel(DMA1, channel->dma_channel);
            found++;
        }
    }

    if (!found)
        log_error("No DMA complete in ISR");
}


void dma1_channel7_isr(void)
{
    process_complete_dma();
}

