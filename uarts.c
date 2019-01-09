#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>


#include "cmd.h"
#include "log.h"
#include "usb.h"



static void uart2_setup()
{
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);

    gpio_mode_setup( GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3 );
    gpio_set_af( GPIOA, GPIO_AF7, GPIO2 | GPIO3 );

    usart_set_baudrate( USART2, 115200 );
    usart_set_databits( USART2, 8 );
    usart_set_stopbits( USART2, USART_CR2_STOPBITS_1 );
    usart_set_mode( USART2, USART_MODE_TX_RX );
    usart_set_parity( USART2, USART_PARITY_NONE );
    usart_set_flow_control( USART2, USART_FLOWCONTROL_NONE );

    nvic_enable_irq(NVIC_USART2_EXTI26_IRQ);
    nvic_set_priority(NVIC_USART2_EXTI26_IRQ, 2);
    usart_enable(USART2);
    usart_enable_rx_interrupt(USART2);
}


void usart2_exti26_isr(void)
{
    usart_wait_recv_ready(USART2);

    char c = usart_recv(USART2);

    cmds_add_char(c);
}




void uarts_setup(void)
{
    uart2_setup();
}
