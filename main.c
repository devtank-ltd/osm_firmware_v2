#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>

static QueueHandle_t uart_txq;


static void
uart_setup(void) {

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);

    gpio_mode_setup( GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3 );
    gpio_set_af( GPIOA, GPIO_AF1, GPIO2 | GPIO3 );
    gpio_set_output_options( GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO2 | GPIO3 );

    usart_set_baudrate( USART2, 115200 );
    usart_set_databits( USART2, 8 );
    usart_set_stopbits( USART2, USART_CR2_STOPBITS_1 );
    usart_set_mode( USART2, USART_MODE_TX_RX );
    usart_set_parity( USART2, USART_PARITY_NONE );
    usart_set_flow_control( USART2, USART_FLOWCONTROL_NONE );

    nvic_enable_irq(NVIC_USART2_IRQ);
    usart_enable(USART2);
    usart_enable_rx_interrupt(USART2);
}


static void uart_send(const char * str)
{
    while(*str)
        usart_send_blocking(USART2, *str++);

    usart_send_blocking(USART2, '\n');
    usart_send_blocking(USART2, '\r');
}


void
usart2_isr(void) {
    usart_wait_recv_ready(USART2);

    char ping[4] = {'-', usart_recv(USART2), '-', 0};

    uart_send(ping);
}



static void
uart_task(void *args __attribute((unused))) {
    uart_send("uart_task");
    for(;;) {
        char ch;
        if ( xQueueReceive(uart_txq, &ch, 10) == pdPASS ) {
            usart_send_blocking(USART2, ch);
        }
        uart_send("[");
    }
}


static inline void
log_msg(const char *s) {
    for ( ; *s; ++s )
        xQueueSend(uart_txq,s,portMAX_DELAY);
    xQueueSend(uart_txq,"\n",portMAX_DELAY);
    xQueueSend(uart_txq,"\r",portMAX_DELAY);
}

static void
task1(void *args __attribute((unused))) {

    uart_send("task1");
    for (;;) {
        gpio_toggle(GPIOA, GPIO5);
        vTaskDelay(pdMS_TO_TICKS(500)); //NOT EVEN PAUSING
        uart_send("loop");
    }
}

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_48mhz();
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    uart_setup();

    uart_txq = xQueueCreate(32,sizeof(char));

    xTaskCreate(uart_task,"UART",200,NULL,configMAX_PRIORITIES-1,NULL); /* Highest priority */
    xTaskCreate(task1,"LED",100,NULL,configMAX_PRIORITIES-2,NULL);

    log_msg("log_out");

    vTaskStartScheduler();

    return 0;
}

