#include <stdlib.h>

#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/syscfg.h>
#include <libopencm3/cm3/nvic.h>

#include "cmd.h"
#include "log.h"
#include "usb_uarts.h"
#include "pinmap.h"
#include "ring.h"
#include "uart_rings.h"
#include "uarts.h"


typedef struct
{
    uint32_t              usart;
    enum rcc_periph_clken uart_clk;
    uint32_t              baud;
    uint32_t              gpioport;
    uint16_t              pins;
    uint8_t               alt_func_num;
    uint8_t               irqn;
    uint32_t              dma_addr;
    uint8_t               dma_irqn;
    uint8_t               dma_channel;
    uint8_t               priority;
} uart_channel_t;

static const uart_channel_t uart_channels[] = UART_CHANNELS;


#define UART_2_ISR  usart2_isr


static void uart_setup(const uart_channel_t * channel)
{
    rcc_periph_clock_enable(PORT_TO_RCC(channel->gpioport));
    rcc_periph_clock_enable(channel->uart_clk);

    gpio_mode_setup( channel->gpioport, GPIO_MODE_AF, GPIO_PUPD_NONE, channel->pins );
    gpio_set_af( channel->gpioport, channel->alt_func_num, channel->pins );

    usart_set_baudrate( channel->usart, channel->baud );
    usart_set_databits( channel->usart, 8 );
    usart_set_stopbits( channel->usart, USART_CR2_STOPBITS_1 );
    usart_set_mode( channel->usart, USART_MODE_TX_RX );
    usart_set_parity( channel->usart, USART_PARITY_NONE );
    usart_set_flow_control( channel->usart, USART_FLOWCONTROL_NONE );

    nvic_set_priority(channel->irqn, channel->priority);
    nvic_enable_irq(channel->irqn);
    usart_enable(channel->usart);
    usart_enable_rx_interrupt(channel->usart);

    if (channel->dma_irqn)
    {
        nvic_set_priority(channel->dma_irqn, channel->priority);
        nvic_enable_irq(channel->dma_irqn);
    }
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

    return ((*c) != 0);
}



static void process_serial(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    char c;

    if (!uart_getc(uart_channels[uart].usart, &c))
    {
        return;
    }

    uart_ring_in(uart, &c, 1);
}


void UART_2_ISR(void)
{
    process_serial(0);
}


#ifdef STM32F0
void usart3_4_isr(void)
{
    process_serial(1);
    process_serial(2);
}
#else
#error Requires handling for UART 3 and 4.
#endif

void uarts_setup(void)
{
    rcc_periph_clock_enable(RCC_DMA);

    for(unsigned n = 0; n < UART_CHANNELS_COUNT; n++)
        uart_setup(&uart_channels[n]);
}


void uart_out(unsigned uart, const char* s, unsigned len)
{
    if (uart >= UART_CHANNELS_COUNT)
        return;

    uart = uart_channels[uart].usart;

    for(unsigned i = 0; i < len; i++)
        usart_send_blocking(uart, s[i]);
}


bool uart_out_async(unsigned uart, char c)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    uart = uart_channels[uart].usart;

    if (!(USART_ISR(uart) & USART_ISR_TXE))
        return false;

    usart_send(uart, c);
    return true;
}

static volatile bool uart_doing_dma[UART_CHANNELS_COUNT] = {0};


bool uart_is_tx_empty(unsigned uart)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    if (uart_doing_dma[uart])
        return false;

    uart = uart_channels[uart].usart;

    return ((USART_ISR(uart) & USART_ISR_TXE));
}


bool uart_dma_out(unsigned uart, char *data, int size)
{
    if (uart >= UART_CHANNELS_COUNT)
        return false;

    if (uart_doing_dma[uart])
        return false;

    const uart_channel_t * channel = &uart_channels[uart];

    if (!(USART_ISR(channel->usart) & USART_ISR_TXE))
        return false;

    if (size == 1)
    {
        usart_send(channel->usart, *data);
        return true;
    }

    if (uart)
        log_debug("UART %u %u out on DMA channel %u", uart, size, channel->dma_channel);

    uart_doing_dma[uart] = true;

    SYSCFG_CFGR1 |= SYSCFG_CFGR1_USART3_DMA_RMP;

    dma_channel_reset(DMA1, channel->dma_channel);

    dma_set_peripheral_address(DMA1, channel->dma_channel, channel->dma_addr);
    dma_set_memory_address(DMA1, channel->dma_channel, (uint32_t)data);
    dma_set_number_of_data(DMA1, channel->dma_channel, size);
    dma_set_read_from_memory(DMA1, channel->dma_channel);
    dma_enable_memory_increment_mode(DMA1, channel->dma_channel);
    dma_set_peripheral_size(DMA1, channel->dma_channel, DMA_CCR_PSIZE_8BIT);
    dma_set_memory_size(DMA1, channel->dma_channel, DMA_CCR_MSIZE_8BIT);
    dma_set_priority(DMA1, channel->dma_channel, DMA_CCR_PL_VERY_HIGH);

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
            if (n)
                log_debug("UART %u DMA channel %u complete", n, channel->dma_channel);

            DMA1_IFCR |= DMA_IFCR_CTCIF(channel->dma_channel);

            uart_doing_dma[n] = false;

            dma_disable_transfer_complete_interrupt(DMA1, channel->dma_channel);

            usart_disable_tx_dma(channel->usart);

            dma_disable_channel(DMA1, channel->dma_channel);
            found++;
        }
    }

    if (!found)
        log_debug("No DMA complete in ISR");
}


void dma1_channel4_7_dma2_channel3_5_isr(void)
{
    process_complete_dma();
}


void dma1_channel2_3_dma2_channel1_2_isr(void)
{
    process_complete_dma();
}
