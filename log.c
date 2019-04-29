#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <libopencm3/stm32/usart.h>


#include "log.h"
#include "uart_rings.h"

bool log_show_debug = true;
bool log_async_log  = false;


extern void platform_raw_msg(const char * s)
{
    while(*s)
        usart_send_blocking(USART2, *s++);

    usart_send_blocking(USART2, '\n');
    usart_send_blocking(USART2, '\r');
}


static void log_msgv(const char *s, va_list ap)
{
    char log_buffer[LOG_LINELEN];
    unsigned len = vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    if (len > LOG_LINELEN)
        len = LOG_LINELEN;

    if (log_async_log)
    {
        cmd_ring_out(log_buffer, len);
        cmd_ring_out("\n\r", 2);
    }
    else platform_raw_msg(log_buffer);
}


void log_debug_raw(const char *s, ...)
{
    if (!log_show_debug)
        return;
    va_list ap;
    va_start(ap, s);
    char log_buffer[LOG_LINELEN];
    vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    va_end(ap);
    log_buffer[LOG_LINELEN-1] = 0;

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
    char log_buffer[LOG_LINELEN];
    unsigned len = vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    if (len > LOG_LINELEN)
        len = LOG_LINELEN;
    va_end(ap);
    uart_ring_out(0, log_buffer, len);
}


void log_error(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(s, ap);
    va_end(ap);
}


void log_init()
{
}
