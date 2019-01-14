#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

#include "cmd.h"
#include "log.h"
#include "usb.h"


typedef struct
{
    uint32_t              usart;
    enum rcc_periph_clken uart_clk;
    enum rcc_periph_clken gpio_clk;
    uint32_t              gpioport;
    uint16_t              pins;
    uint8_t               alt_func_num;
    uint8_t               irqn;
    uint32_t              baud;
    uint8_t               priority;
} uart_channel_t;


static const uart_channel_t uart_channels[] = {
    { USART2, RCC_USART2, RCC_GPIOA, GPIOA, GPIO2  | GPIO3,  GPIO_AF7, NVIC_USART2_EXTI26_IRQ, 115200, UART1_PRIORITY },
    { USART1, RCC_USART1, RCC_GPIOA, GPIOA, GPIO9  | GPIO10, GPIO_AF7, NVIC_USART1_EXTI25_IRQ, 115200, UART2_PRIORITY },
    { USART3, RCC_USART3, RCC_GPIOB, GPIOB, GPIO8  | GPIO9,  GPIO_AF7, NVIC_USART3_EXTI28_IRQ, 9600  , UART3_PRIORITY },
    { UART4,  RCC_UART4,  RCC_GPIOC, GPIOC, GPIO10 | GPIO11, GPIO_AF5, NVIC_UART4_EXTI34_IRQ,  9600  , UART4_PRIORITY },
};


static void uart_setup(const uart_channel_t * channel)
{
    rcc_periph_clock_enable(channel->gpio_clk);
    rcc_periph_clock_enable(channel->uart_clk);

    gpio_mode_setup( channel->gpioport, GPIO_MODE_AF, GPIO_PUPD_NONE, channel->pins );
    gpio_set_af( channel->gpioport, channel->alt_func_num, channel->pins );

    usart_set_baudrate( channel->usart, channel->baud );
    usart_set_databits( channel->usart, 8 );
    usart_set_stopbits( channel->usart, USART_CR2_STOPBITS_1 );
    usart_set_mode( channel->usart, USART_MODE_TX_RX );
    usart_set_parity( channel->usart, USART_PARITY_NONE );
    usart_set_flow_control( channel->usart, USART_FLOWCONTROL_NONE );

    nvic_enable_irq(channel->irqn);
    nvic_set_priority(channel->irqn, channel->priority);
    usart_enable(channel->usart);
    usart_enable_rx_interrupt(channel->usart);
}



static bool uart_getc(uint32_t uart, char* c)
{
    uint32_t flags = USART_ISR(uart);

    if (!(flags & USART_ISR_RXNE))
    {
        USART_ICR(uart) = flags;
        return false;
    }

    usart_wait_recv_ready(uart);

    *c = usart_recv(uart);

    return ((*c) != 0);
}


void usart2_exti26_isr(void)
{
    char c;
    if (uart_getc(USART2, &c))
        cmds_add_char(c);
}


static void process_serial(unsigned uart)
{
    if (uart > 3)
        return;

    char c;

    if (!uart_getc(uart_channels[uart].usart, &c))
    {
        return;
    }

    usb_uart_send(uart - 1, &c, 1);
}


void usart1_exti25_isr(void) { process_serial(1); }
void usart3_exti28_isr(void) { process_serial(2); }
void uart4_exti34_isr(void)  { process_serial(3); }


void uarts_setup(void)
{
    for(unsigned n = 0; n < 4; n++)
        uart_setup(&uart_channels[n]);
}


void uart_out(unsigned uart, const char* s, unsigned len)
{
    if (uart > 3)
        return;

    uart = uart_channels[uart].usart;

    for (unsigned n = 0; n < len; n++)
        usart_send_blocking(uart, s[n]);
}
