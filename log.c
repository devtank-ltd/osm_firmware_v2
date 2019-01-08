#include <stdarg.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <libopencm3/stm32/usart.h>

#include "log.h"

bool log_show_debug = false;
bool log_async = false;


static QueueHandle_t log_txq;


extern void platform_raw_msg(const char * s)
{

    for (unsigned n = 0; *s && n < LOG_LINELEN; n++)
        usart_send_blocking(USART2, *s++);

    usart_send_blocking(USART2, '\n');
    usart_send_blocking(USART2, '\r');
}


void log_raw(const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    char log_buffer[LOG_LINELEN];

    vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    platform_raw_msg(log_buffer);
    va_end(ap);
}


static void log_msgv(const char *s, va_list ap)
{
    char log_buffer[LOG_LINELEN];

    vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;

    if (log_async)
        xQueueSend(log_txq, log_buffer, portMAX_DELAY);
    else
        platform_raw_msg(log_buffer);
}


void log_out(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(s, ap);
    va_end(ap);
}


void log_debug(const char *s, ...)
{
    if (!log_show_debug)
        return;
    va_list ap;
    va_start(ap, s);
    log_msgv(s, ap);
    va_end(ap);
}


void log_error(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(s, ap);
    va_end(ap);
}


static void log_task(void *args __attribute((unused)))
{
    log_debug("log_task");
    for(;;)
    {
        char tmp[MAX_LINELEN];
        if ( xQueueReceive(log_txq, tmp, portMAX_DELAY) == pdPASS )
            platform_raw_msg(tmp);
    }
}



void log_init()
{
    log_txq = xQueueCreate(4,MAX_LINELEN);

    xTaskCreate(log_task,"LOG",200,NULL,configMAX_PRIORITIES-1,NULL);
}
