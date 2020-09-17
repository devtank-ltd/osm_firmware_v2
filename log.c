#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <libopencm3/stm32/usart.h>


#include "log.h"
#include "uarts.h"
#include "uart_rings.h"

uint32_t log_debug_mask = DEBUG_SYS;
bool     log_async_log  = false;


extern void platform_raw_msg(const char * s)
{
    uart_blocking(UART_ERR_NU, s, strlen(s));
    uart_blocking(UART_ERR_NU, "\n\r", 2);
}


static void log_msgv(unsigned uart, bool blocking, const char *s, va_list ap)
{
    char log_buffer[LOG_LINELEN];
    unsigned len = vsnprintf(log_buffer, LOG_LINELEN, s, ap);
    log_buffer[LOG_LINELEN-1] = 0;
    if (len > LOG_LINELEN-1)
        len = LOG_LINELEN-1;

    if (!blocking)
    {
        uart_ring_out(uart, log_buffer, len);
        uart_ring_out(uart, "\n\r", 2);
    }
    else
    {
        uart_blocking(uart, log_buffer, len);
        uart_blocking(uart, "\n\r", 2);
    }
}


void log_debug(uint32_t flag, const char *s, ...)
{
    if (!(flag & log_debug_mask))
        return;

    va_list ap;
    va_start(ap, s);
    log_msgv(UART_ERR_NU, true, s, ap);
    va_end(ap);
}


void log_uart(unsigned uart, bool blocking, const char * s, ...)
{
    va_list ap;
    va_start(ap, s);
    log_msgv(uart, blocking, s, ap);
    va_end(ap);
}
