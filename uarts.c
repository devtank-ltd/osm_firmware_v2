#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>


#include "cmd.h"
#include "log.h"
#include "usb.h"



static void uart_setup(uint32_t usart,
                       enum rcc_periph_clken uart_clk,
                       enum rcc_periph_clken gpio_clk,
                       uint16_t pins,
                       uint8_t alt_func_num,
                       uint8_t irqn,
                       uint32_t baud
                       )
{
    rcc_periph_clock_enable(gpio_clk);
    rcc_periph_clock_enable(uart_clk);

    gpio_mode_setup( GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, pins );
    gpio_set_af( GPIOA, alt_func_num, pins );

    usart_set_baudrate( usart, baud );
    usart_set_databits( usart, 8 );
    usart_set_stopbits( usart, USART_CR2_STOPBITS_1 );
    usart_set_mode( usart, USART_MODE_TX_RX );
    usart_set_parity( usart, USART_PARITY_NONE );
    usart_set_flow_control( usart, USART_FLOWCONTROL_NONE );

    nvic_enable_irq(irqn);
    nvic_set_priority(irqn, 2);
    usart_enable(usart);
    usart_enable_rx_interrupt(usart);
}




static void uart2_setup()
{
    uart_setup(USART2, RCC_USART2, RCC_GPIOA, GPIO2 | GPIO3, GPIO_AF7, NVIC_USART2_EXTI26_IRQ, 115200);
}


void usart2_exti26_isr(void)
{
    usart_wait_recv_ready(USART2);

    char c = usart_recv(USART2);

    cmds_add_char(c);
}



static void uart1_setup()
{
    uart_setup(USART1, RCC_USART1, RCC_GPIOA, GPIO9 | GPIO10, GPIO_AF7, NVIC_USART1_EXTI25_IRQ, 115200);
}


void usart1_exti25_isr(void)
{
    usart_wait_recv_ready(USART1);

    char c = usart_recv(USART1);

    log_out("USART1 : %c", c);
}



static void uart3_setup()
{
    uart_setup(USART3, RCC_USART3, RCC_GPIOB, GPIO8 | GPIO9, GPIO_AF7, NVIC_USART3_EXTI28_IRQ, 9600);
}


void usart3_exti28_isr(void)
{
    usart_wait_recv_ready(USART3);

    char c = usart_recv(USART3);

    log_out("UART3 : %c", c);
}


static void uart4_setup()
{
    uart_setup(UART4, RCC_UART4, RCC_GPIOC, GPIO10 | GPIO11, GPIO_AF5, NVIC_UART4_EXTI34_IRQ, 9600);
}


void uart4_exti34_isr(void)
{
    usart_wait_recv_ready(UART4);

    char c = usart_recv(UART4);

    log_out("UART4 : %c", c);
}


void uarts_setup(void)
{
    uart1_setup();
    uart2_setup();
    uart3_setup();
    uart4_setup();
}
