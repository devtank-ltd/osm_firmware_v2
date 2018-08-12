#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

static QueueHandle_t uart_txq;


extern void xPortPendSVHandler( void ) __attribute__ (( naked ));
extern void xPortSysTickHandler( void );


extern void raw_log_msg(const char * str)
{
    while(*str)
        usart_send_blocking(USART2, *str++);

    usart_send_blocking(USART2, '\n');
    usart_send_blocking(USART2, '\r');
}

void
vApplicationStackOverflowHook(xTaskHandle *pxTask,signed portCHAR *pcTaskName) {
    (void)pxTask;
    (void)pcTaskName;
    raw_log_msg("----big fat FreeRTOS crash -----");
    while(true) {
        for(unsigned n = 0; n < 1000000; n++)
            asm("nop");
        gpio_toggle(GPIOA, GPIO5);
    }
}


void hard_fault_handler(void)
{
    raw_log_msg("----big fat libopen3 crash -----");
    while(true);
}


void pend_sv_handler(void) {
    xPortPendSVHandler();
}

void sys_tick_handler(void) {
    xPortSysTickHandler();
}

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


static inline void
log_msg(const char *s) {
    for ( ; *s; ++s )
        xQueueSend(uart_txq,s,portMAX_DELAY);
    xQueueSend(uart_txq,"\n",portMAX_DELAY);
    xQueueSend(uart_txq,"\r",portMAX_DELAY);
}


void
usart2_isr(void) {
    usart_wait_recv_ready(USART2);

    char ping[4] = {'-', usart_recv(USART2), '-', 0};

    log_msg(ping);
}



static void
uart_task(void *args __attribute((unused))) {
    log_msg("uart_task");
    for(;;) {
        char ch;
        if ( xQueueReceive(uart_txq, &ch, portMAX_DELAY) == pdPASS ) {
            usart_send_blocking(USART2, ch);
        }
    }
}


static void
blink_task(void *args __attribute((unused))) {
    log_msg("blink_task");
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    for (;;) {
        gpio_toggle(GPIOA, GPIO5);
        log_msg("flash-pre");
        vTaskDelay(pdMS_TO_TICKS(500));
        log_msg("flash-post");
    }
}


int main(void) {
    rcc_clock_setup_in_hse_8mhz_out_48mhz();
    uart_setup();
    rcc_periph_clock_enable(RCC_GPIOA);

    uart_txq = xQueueCreate(32,sizeof(char));

    systick_interrupt_enable();
    systick_counter_enable();

    raw_log_msg("----start----");

    xTaskCreate(uart_task,"UART",200,NULL,configMAX_PRIORITIES-1,NULL);
    xTaskCreate(blink_task,"LED",100,NULL,configMAX_PRIORITIES-2,NULL);

    vTaskStartScheduler();

    return 0;
}

