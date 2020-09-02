#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <libopencm3/stm32/usart.h>


#include "log.h"
#include "uart_rings.h"

uint32_t log_debug_mask = 0;
bool     log_async_log  = false;


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
    if (len > LOG_LINELEN-1)
        len = LOG_LINELEN-1;

    if (log_async_log)
    {
        cmd_ring_out(log_buffer, len);
        cmd_ring_out("\n\r", 2);
    }
    else platform_raw_msg(log_buffer);
}


void log_out(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(s, ap);
    va_end(ap);
}


void log_errorv(const char *s, va_list ap)
{
    char log_buffer[LOG_LINELEN];
    unsigned len = vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    if (len > LOG_LINELEN-1)
        len = LOG_LINELEN-1;

    if (log_async_log)
    {
        uart_ring_out(0, log_buffer, len);
        uart_ring_out(0, "\r\n", 2);
    }
    else platform_raw_msg(log_buffer);
}


void log_error(const char *s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_errorv(s, ap);
    va_end(ap);
}


void log_debug(uint32_t flag, const char *s, ...)
{
    if (!(flag & log_debug_mask))
        return;

    va_list ap;
    va_start(ap, s);
    log_errorv(s, ap);
    va_end(ap);
}


void log_init()
{
}
